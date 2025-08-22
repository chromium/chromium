// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/browser_management_service.h"

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_status_provider.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/image/image.h"

namespace policy {

namespace {

std::vector<std::unique_ptr<ManagementStatusProvider>>
GetManagementStatusProviders(Profile* profile) {
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
  providers.emplace_back(
      std::make_unique<BrowserCloudManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<LocalBrowserManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<LocalDomainBrowserManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<ProfileCloudManagementStatusProvider>(profile));
  providers.emplace_back(
      std::make_unique<LocalTestPolicyUserManagementProvider>(profile));
  providers.emplace_back(
      std::make_unique<LocalTestPolicyBrowserManagementProvider>(profile));
#if BUILDFLAG(IS_CHROMEOS)
  providers.emplace_back(std::make_unique<DeviceManagementStatusProvider>());
#endif
  return providers;
}

}  // namespace

BrowserManagementService::BrowserManagementService(Profile* profile)
    : ManagementService(GetManagementStatusProviders(profile)) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserManagementService::UpdateManagementIconForProfile,
                     weak_ptr_factory_.GetWeakPtr(), profile));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserManagementService::UpdateManagementIconForBrowser,
                     weak_ptr_factory_.GetWeakPtr(), profile));
  UpdateEnterpriseLabelForProfile(profile);
  StartListeningToPrefChanges(profile);

  policy::CloudPolicyManager* cloud_policy_manager =
      profile->GetCloudPolicyManager();
  if (cloud_policy_manager) {
    provider_ = std::make_unique<UserCloudPolicyStatusProvider>(
        cloud_policy_manager->core(), profile);
    policy_status_provider_observations_.Observe(provider_.get());
  }

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

ui::ImageModel* BrowserManagementService::GetManagementIconForProfile() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return management_icon_for_profile_.IsEmpty() ? nullptr
                                                : &management_icon_for_profile_;
#else
  return nullptr;
#endif
}

gfx::Image* BrowserManagementService::GetManagementIconForBrowser() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return management_icon_for_browser_.IsEmpty() ? nullptr
                                                : &management_icon_for_browser_;
#else
  return nullptr;
#endif
}

void BrowserManagementService::TriggerPolicyStatusChangedForTesting() {
  CHECK_IS_TEST();
  OnPolicyStatusChanged();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void BrowserManagementService::SetBrowserManagementIconForTesting(
    const gfx::Image& management_icon) {
  CHECK_IS_TEST();
  management_icon_for_browser_ = management_icon;
}

void BrowserManagementService::StartListeningToPrefChanges(Profile* profile) {
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kEnterpriseLogoUrlForProfile,
      base::BindRepeating(
          &BrowserManagementService::UpdateManagementIconForProfile,
          weak_ptr_factory_.GetWeakPtr(), profile));
  pref_change_registrar_.Add(
      prefs::kEnterpriseCustomLabelForProfile,
      base::BindRepeating(
          &BrowserManagementService::UpdateEnterpriseLabelForProfile,
          weak_ptr_factory_.GetWeakPtr(), profile));
  pref_change_registrar_.Add(
      prefs::kEnterpriseProfileBadgeToolbarSettings,
      base::BindRepeating(
          &BrowserManagementService::UpdateEnterpriseLabelForProfile,
          weak_ptr_factory_.GetWeakPtr(), profile));

  auto* browser_process = g_browser_process->local_state();
  if (browser_process) {
    local_state_pref_change_registrar_.Init(g_browser_process->local_state());
    local_state_pref_change_registrar_.Add(
        prefs::kEnterpriseLogoUrlForBrowser,
        base::BindRepeating(
            &BrowserManagementService::UpdateManagementIconForBrowser,
            weak_ptr_factory_.GetWeakPtr(), profile));
  }
}

void BrowserManagementService::UpdateManagementIconForProfile(
    Profile* profile) {
  enterprise_util::GetManagementIcon(
      GURL(profile->GetPrefs()->GetString(prefs::kEnterpriseLogoUrlForProfile)),
      profile,
      enterprise_util::EnterpriseLogoUrlScope::kProfile,
      base::BindOnce(&BrowserManagementService::SetManagementIconForProfile,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrowserManagementService::UpdateManagementIconForBrowser(
    Profile* profile) {
  if (!g_browser_process->local_state() ||
      !g_browser_process->local_state()->FindPreference(
          prefs::kEnterpriseLogoUrlForBrowser)) {
    // Can be NULL in tests.
    CHECK_IS_TEST();
    return;
  }

  std::string logo_url = g_browser_process->local_state()->GetString(
      prefs::kEnterpriseLogoUrlForBrowser);
  if (logo_url.empty()) {
    SetManagementIconForBrowser(gfx::Image());
    return;
  }
  enterprise_util::GetManagementIcon(
      GURL(logo_url), profile,
      enterprise_util::EnterpriseLogoUrlScope::kBrowser,
      base::BindOnce(&BrowserManagementService::SetManagementIconForBrowser,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrowserManagementService::UpdateEnterpriseLabelForProfile(
    Profile* profile) {
  enterprise_util::SetEnterpriseProfileLabel(profile);
  NotifyEnterpriseLabelUpdated();
}

void BrowserManagementService::SetManagementIconForProfile(
    const gfx::Image& management_icon) {
  management_icon_for_profile_ = ui::ImageModel::FromImage(management_icon);
}

void BrowserManagementService::SetManagementIconForBrowser(
    const gfx::Image& management_icon) {
  management_icon_for_browser_ = management_icon;
  NotifyEnterpriseLogoForBrowserUpdated();
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

void BrowserManagementService::OnPolicyStatusChanged() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  NotifyEnterpriseLabelUpdated();
#endif
}

BrowserManagementService::~BrowserManagementService() = default;

}  // namespace policy
