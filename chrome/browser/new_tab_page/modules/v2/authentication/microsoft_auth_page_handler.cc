// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/authentication/microsoft_auth_page_handler.h"

#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"

// static
const char MicrosoftAuthPageHandler::kLastDismissedTimePrefName[] =
    "NewTabPage.MicrosoftAuthentication.LastDimissedTime";

// static
const base::TimeDelta MicrosoftAuthPageHandler::kDismissDuration =
    base::Hours(12);

// static
void MicrosoftAuthPageHandler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kLastDismissedTimePrefName, base::Time());
}

MicrosoftAuthPageHandler::MicrosoftAuthPageHandler(
    mojo::PendingReceiver<ntp::authentication::mojom::MicrosoftAuthPageHandler>
        handler,
    Profile* profile)
    : handler_(this, std::move(handler)),
      profile_(profile),
      pref_service_(profile_->GetPrefs()) {}

MicrosoftAuthPageHandler::~MicrosoftAuthPageHandler() = default;

void MicrosoftAuthPageHandler::ShouldShowModule(
    ShouldShowModuleCallback callback) {
  const base::Time last_dismissed_time =
      pref_service_->GetTime(kLastDismissedTimePrefName);
  if (!last_dismissed_time.is_null()) {
    base::TimeDelta elapsed_time = base::Time::Now() - last_dismissed_time;
    bool still_dismissed = elapsed_time < kDismissDuration;
    if (still_dismissed) {
      std::move(callback).Run(false);
      const std::string remaining_hours =
          base::NumberToString((kDismissDuration - elapsed_time).InHours());
      LogModuleDismissed(ntp_features::kNtpMicrosoftAuthenticationModule, true,
                         remaining_hours);
      return;
    }
  }

  std::move(callback).Run(true);
  LogModuleDismissed(ntp_features::kNtpMicrosoftAuthenticationModule, false,
                     /*remaining_hours=*/"0");
}

void MicrosoftAuthPageHandler::DismissModule() {
  pref_service_->SetTime(kLastDismissedTimePrefName, base::Time::Now());
}

void MicrosoftAuthPageHandler::RestoreModule() {
  pref_service_->SetTime(kLastDismissedTimePrefName, base::Time());
}
