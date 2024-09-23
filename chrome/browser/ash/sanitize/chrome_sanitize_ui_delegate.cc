// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sanitize/chrome_sanitize_ui_delegate.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/ash/sanitize/chrome_sanitize_ui_delegate.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

ChromeSanitizeUIDelegate::~ChromeSanitizeUIDelegate() = default;

ChromeSanitizeUIDelegate::ChromeSanitizeUIDelegate(content::WebUI* web_ui) {
  auto* profile = Profile::FromWebUI(web_ui);
  resetter_ = std::make_unique<ProfileResetter>(profile);
  pref_service_ = profile->GetPrefs();
}

void ChromeSanitizeUIDelegate::PerformSanitizeSettings() {
  ProfileResetter::ResettableFlags to_sanitize =
      ProfileResetter::HOMEPAGE | ProfileResetter::CONTENT_SETTINGS |
      ProfileResetter::EXTENSIONS | ProfileResetter::STARTUP_PAGES |
      ProfileResetter::PINNED_TABS | ProfileResetter::SHORTCUTS |
      ProfileResetter::NTP_CUSTOMIZATIONS | ProfileResetter::LANGUAGES |
      ProfileResetter::DNS_CONFIGURATIONS;

  GetResetter()->ResetSettings(
      to_sanitize, nullptr,
      base::BindOnce(&ChromeSanitizeUIDelegate::OnSanitizeDone,
                     base::Unretained(this)));

  base::RecordAction(base::UserMetricsAction("Sanitize"));
}

ProfileResetter* ChromeSanitizeUIDelegate::GetResetter() {
  return resetter_.get();
}

void ChromeSanitizeUIDelegate::RestartChrome() {
  chrome::AttemptRestart();
}

void ChromeSanitizeUIDelegate::OnSanitizeDone() {
  pref_service_->SetBoolean(ash::settings::prefs::kSanitizeCompleted, true);
  pref_service_->CommitPendingWrite();
  RestartChrome();
}

}  // namespace ash
