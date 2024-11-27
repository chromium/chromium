// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar_page_handler.h"

#include <string>
#include <vector>

#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_data.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_fake_data_helper.h"
#include "components/search/ntp_features.h"

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
    std::move(callback).Run(calendar::calendar_fake_data_helper::GetFakeEvents(
        calendar::calendar_fake_data_helper::CalendarType::OUTLOOK_CALENDAR));
  } else {
    MakeRequest(std::move(callback));
  }
}

void OutlookCalendarPageHandler::MakeRequest(GetEventsCallback callback) {
  // TODO(357700028): Replace fake JSON response with an actual HTTP
  // request/response.
  calendar::calendar_fake_data_helper::GetFakeJsonResponse(
      base::BindOnce(&OutlookCalendarPageHandler::OnJsonReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void OutlookCalendarPageHandler::OnJsonReceived(GetEventsCallback callback,
                                                std::string response_body) {
  if (!response_body.empty()) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        response_body,
        base::BindOnce(&OutlookCalendarPageHandler::OnJsonParsed,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
  }
}

void OutlookCalendarPageHandler::OnJsonParsed(
    GetEventsCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
    return;
  }
  auto* events = result->GetDict().FindList("value");
  if (!events) {
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
    return;
  }

  std::vector<ntp::calendar::mojom::CalendarEventPtr> created_events;
  for (const auto& event : *events) {
    const auto& event_dict = event.GetDict();
    std::optional<bool> has_attachments = event_dict.FindBool("hasAttachments");
    const std::string* title = event_dict.FindString("subject");
    const std::string* calendar_url = event_dict.FindString("webLink");
    const std::string* conference_url =
        event_dict.FindStringByDottedPath("onlineMeeting.joinUrl");
    const std::string* location =
        event_dict.FindStringByDottedPath("location.displayName");
    const std::string* start_time =
        event_dict.FindStringByDottedPath("start.dateTime");
    const std::string* end_time =
        event_dict.FindStringByDottedPath("end.dateTime");
    std::optional<bool> is_organizer = event_dict.FindBool("isOrganizer");
    const base::Value::List* attendees = event_dict.FindList("attendees");

    base::Time start_timestamp;
    base::Time end_timestamp;

    // Do not send calendar event data if all required information is not
    // found in the response.
    if (!has_attachments.has_value() || !title || !calendar_url ||
        !start_time || !end_time || !is_organizer.has_value() || !attendees ||
        !location ||
        !base::Time::FromUTCString((*start_time).c_str(), &start_timestamp) ||
        !base::Time::FromUTCString((*end_time).c_str(), &end_timestamp)) {
      std::move(callback).Run(
          std::vector<ntp::calendar::mojom::CalendarEventPtr>());
      return;
    }
    ntp::calendar::mojom::CalendarEventPtr created_event =
        ntp::calendar::mojom::CalendarEvent::New();
    created_event->title = *title;
    created_event->start_time = start_timestamp;
    created_event->end_time = end_timestamp;
    created_event->url = GURL(*calendar_url);
    // TODO(357700028) Handle case where the user is not the event organizer.
    created_event->is_accepted = is_organizer.value();
    // TODO(357700028): Filter out attendees that declined.
    // Note: If user is the organizer they are not found in the attendees list.
    created_event->has_other_attendee = !(*attendees).empty();
    created_event->location = *location;
    if (conference_url) {
      created_event->conference_url = GURL(*conference_url);
    }
    created_events.push_back(std::move(created_event));
  }
  std::move(callback).Run(std::move(created_events));
}

// TODO(357700028): Delete once the end-to-end HTTP request implementation is
// done.
void OutlookCalendarPageHandler::GetEventsForTesting(
    GetEventsCallback callback,
    std::string response_body) {
  // Bypass "making a request" and trigger response received.
  OnJsonReceived(std::move(callback), response_body);
}
