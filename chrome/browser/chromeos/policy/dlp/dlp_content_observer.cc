// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/dlp/dlp_content_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_lacros.h"
#endif

namespace policy {

namespace {
static DlpContentObserver* g_testing_dlp_content_observer = nullptr;
}  // namespace

// static
DlpContentObserver* DlpContentObserver::Get() {
  if (g_testing_dlp_content_observer)
    return g_testing_dlp_content_observer;

    // Initializes DlpContentManager(Lacros) if needed.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return DlpContentManager::Get();
#else
  return DlpContentManagerLacros::Get();
#endif
}

DlpContentRestrictionSet DlpContentObserver::GetRestrictionSetForURL(
    const GURL& url) const {
  DlpContentRestrictionSet set;

  // TODO(crbug.com/1254329) Enable on LaCros once DlpRulesManager is available.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DlpRulesManager* dlp_rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!dlp_rules_manager)
    return set;

  const size_t kRestrictionsCount = 5;
  static constexpr std::array<
      std::pair<DlpRulesManager::Restriction, DlpContentRestriction>,
      kRestrictionsCount>
      kRestrictionsArray = {{{DlpRulesManager::Restriction::kScreenshot,
                              DlpContentRestriction::kScreenshot},
                             {DlpRulesManager::Restriction::kScreenshot,
                              DlpContentRestriction::kVideoCapture},
                             {DlpRulesManager::Restriction::kPrivacyScreen,
                              DlpContentRestriction::kPrivacyScreen},
                             {DlpRulesManager::Restriction::kPrinting,
                              DlpContentRestriction::kPrint},
                             {DlpRulesManager::Restriction::kScreenShare,
                              DlpContentRestriction::kScreenShare}}};

  for (const auto& restriction : kRestrictionsArray) {
    DlpRulesManager::Level level =
        dlp_rules_manager->IsRestricted(url, restriction.first);
    if (level == DlpRulesManager::Level::kNotSet ||
        level == DlpRulesManager::Level::kAllow)
      continue;
    set.SetRestriction(restriction.second, level, url);
  }
#endif

  return set;
}

/* static */
void DlpContentObserver::SetDlpContentObserverForTesting(
    DlpContentObserver* dlp_content_observer) {
  if (g_testing_dlp_content_observer)
    delete g_testing_dlp_content_observer;
  g_testing_dlp_content_observer = dlp_content_observer;
}

/* static */
void DlpContentObserver::ResetDlpContentObserverForTesting() {
  g_testing_dlp_content_observer = nullptr;
}

// ScopedDlpContentObserverForTesting
ScopedDlpContentObserverForTesting::ScopedDlpContentObserverForTesting(
    DlpContentObserver* test_dlp_content_observer) {
  DlpContentObserver::SetDlpContentObserverForTesting(
      test_dlp_content_observer);
}

ScopedDlpContentObserverForTesting::~ScopedDlpContentObserverForTesting() {
  DlpContentObserver::ResetDlpContentObserverForTesting();
}

}  // namespace policy
