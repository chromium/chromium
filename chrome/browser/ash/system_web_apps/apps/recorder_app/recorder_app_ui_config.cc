// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/recorder_app/recorder_app_ui_config.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/recorder_app_ui/recorder_app_ui.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/system_web_apps/apps/recorder_app/chrome_recorder_app_ui_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/consent_auditor/consent_auditor_provider.h"
#include "chromeos/ash/components/media_device_salt/media_device_salt_service_provider.h"
#include "chromeos/ash/components/signin/identity_manager_provider.h"
#include "components/soda/soda_features.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/common/url_constants.h"

namespace ash {

RecorderAppUIConfig::RecorderAppUIConfig(
    PrefService* local_state,
    const ApplicationLocaleStorage* application_locale_storage,
    variations::VariationsService* variations_service)
    : WebUIConfig(content::kChromeUIScheme, kChromeUIRecorderAppHost),
      local_state_(CHECK_DEREF(local_state)),
      application_locale_storage_(CHECK_DEREF(application_locale_storage)),
      variations_service_(variations_service) {
  CHECK(variations_service_);
  variations_service_observation_.Observe(variations_service_);
}

RecorderAppUIConfig::~RecorderAppUIConfig() = default;

bool RecorderAppUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kConch) ||
         base::FeatureList::IsEnabled(
             speech::kFeatureManagementCrosSodaConchLanguages);
}

std::unique_ptr<content::WebUIController>
RecorderAppUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                           const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  const AccountId& account_id =
      CHECK_DEREF(AnnotatedAccountId::Get(profile->GetOriginalProfile()));
  // The providers resolve a guest's AccountId to the off-the-record profile,
  // which has no IdentityManager and which ConsentAuditorFactory rejects.
  signin::IdentityManager* identity_manager = nullptr;
  consent_auditor::ConsentAuditor* consent_auditor = nullptr;
  if (account_id != user_manager::GuestAccountId()) {
    identity_manager = IdentityManagerProvider::Get().Find(account_id);
    CHECK(identity_manager);
    consent_auditor = ConsentAuditorProvider::Get().Find(account_id);
    CHECK(consent_auditor);
  }
  media_device_salt::MediaDeviceSaltService* media_device_salt_service =
      MediaDeviceSaltServiceProvider::Get().Find(account_id);

  CHECK(variations_service_);
  auto delegate = std::make_unique<ChromeRecorderAppUIDelegate>(
      &application_locale_storage_.get(), variations_service_.get(),
      user_manager::UserManager::Get(), account_id, identity_manager,
      consent_auditor);
  return std::make_unique<RecorderAppUI>(&local_state_.get(), web_ui,
                                         std::move(delegate),
                                         media_device_salt_service);
}

void RecorderAppUIConfig::OnVariationsServiceDestroyed() {
  // `VariationsService` is destroyed in `BrowserProcessImpl::StartTearDown()`,
  // before `AshWebUIConfigManager` unregisters this config.
  variations_service_observation_.Reset();
  variations_service_ = nullptr;
}

}  // namespace ash
