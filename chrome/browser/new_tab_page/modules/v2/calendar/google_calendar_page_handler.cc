// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar_page_handler.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"

namespace {

const char kGoogleCalendarLastDismissedTimePrefName[] =
    "NewTabPage.GoogleCalendar.LastDimissedTime";

}  // namespace

// static
void GoogleCalendarPageHandler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  if (base::FeatureList::IsEnabled(ntp_features::kNtpCalendarModule)) {
    registry->RegisterTimePref(kGoogleCalendarLastDismissedTimePrefName,
                               base::Time());
  }
}

GoogleCalendarPageHandler::GoogleCalendarPageHandler(
    mojo::PendingReceiver<ntp::calendar::mojom::GoogleCalendarPageHandler>
        handler,
    Profile* profile)
    : handler_(this, std::move(handler)),
      profile_(profile),
      pref_service_(profile_->GetPrefs()) {}

GoogleCalendarPageHandler::~GoogleCalendarPageHandler() = default;

void GoogleCalendarPageHandler::DismissModule() {
  pref_service_->SetTime(kGoogleCalendarLastDismissedTimePrefName,
                         base::Time::Now());
}

void GoogleCalendarPageHandler::RestoreModule() {
  pref_service_->SetTime(kGoogleCalendarLastDismissedTimePrefName,
                         base::Time());
}
