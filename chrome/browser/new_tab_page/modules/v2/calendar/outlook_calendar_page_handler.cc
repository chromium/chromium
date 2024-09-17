// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar_page_handler.h"

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_data.mojom.h"
#include "components/search/ntp_features.h"

namespace {

// TODO (5839327): Replace with MSFT SharePoint file icon.
const char kCalendarFileIconUrl[] =
    "https://drive-thirdparty.googleusercontent.com/16/type/application/"
    "vnd.google-apps.document";

std::vector<ntp::calendar::mojom::CalendarEventPtr> GetFakeEvents() {
  std::vector<ntp::calendar::mojom::CalendarEventPtr> events;
  for (int i = 0; i < 5; ++i) {
    ntp::calendar::mojom::CalendarEventPtr event =
        ntp::calendar::mojom::CalendarEvent::New();
    event->title = "Calendar Event " + base::NumberToString(i);
    event->start_time = base::Time::Now() + base::Minutes(i * 30);
    event->end_time = event->start_time + base::Minutes(30);
    event->url = GURL("https://foo.com/" + base::NumberToString(i));
    event->location = "Conference Room " + base::NumberToString(i);
    for (int j = 0; j < 3; ++j) {
      ntp::calendar::mojom::AttachmentPtr attachment =
          ntp::calendar::mojom::Attachment::New();
      attachment->title = "Attachment " + base::NumberToString(j);
      attachment->resource_url =
          GURL("https://foo.com/attachment" + base::NumberToString(j));
      attachment->icon_url = GURL(kCalendarFileIconUrl);
      event->attachments.push_back(std::move(attachment));
    }
    event->conference_url =
        GURL("https://foo.com/conference" + base::NumberToString(i));
    event->is_accepted = true;
    event->has_other_attendee = false;
    events.push_back(std::move(event));
  }
  return events;
}

}  // namespace

OutlookCalendarPageHandler::OutlookCalendarPageHandler(
    mojo::PendingReceiver<ntp::calendar::mojom::OutlookCalendarPageHandler>
        handler)
    : handler_(this, std::move(handler)) {}

OutlookCalendarPageHandler::~OutlookCalendarPageHandler() = default;

void OutlookCalendarPageHandler::GetEvents(GetEventsCallback callback) {
  const std::string fake_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpOutlookCalendarModule,
      ntp_features::kNtpOutlookCalendarModuleDataParam);
  if (!fake_data_param.empty()) {
    std::move(callback).Run(GetFakeEvents());
  } else {
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
  }
}
