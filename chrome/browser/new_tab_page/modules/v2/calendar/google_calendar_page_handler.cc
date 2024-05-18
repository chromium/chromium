// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar_page_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"

namespace {

const char kGoogleCalendarLastDismissedTimePrefName[] =
    "NewTabPage.GoogleCalendar.LastDimissedTime";

ntp::calendar::mojom::CalendarEventPtr GetFakeEvent(int index) {
  ntp::calendar::mojom::CalendarEventPtr event =
      ntp::calendar::mojom::CalendarEvent::New();
  event->title = "Calendar Event " + base::NumberToString(index);
  return event;
}

std::vector<ntp::calendar::mojom::CalendarEventPtr> GetFakeEvents() {
  std::vector<ntp::calendar::mojom::CalendarEventPtr> events;
  for (int i = 0; i < 3; ++i) {
    events.push_back(GetFakeEvent(i));
  }
  return events;
}

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

void GoogleCalendarPageHandler::GetEvents(GetEventsCallback callback) {
  const std::string fake_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpCalendarModule,
      ntp_features::kNtpCalendarModuleDataParam);
  if (!fake_data_param.empty()) {
    std::move(callback).Run(GetFakeEvents());
  } else {
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
  }
}

void GoogleCalendarPageHandler::DismissModule() {
  pref_service_->SetTime(kGoogleCalendarLastDismissedTimePrefName,
                         base::Time::Now());
}

void GoogleCalendarPageHandler::RestoreModule() {
  pref_service_->SetTime(kGoogleCalendarLastDismissedTimePrefName,
                         base::Time());
}
