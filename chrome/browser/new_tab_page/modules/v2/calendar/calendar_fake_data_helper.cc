// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_fake_data_helper.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace {

const char kGoogleCalendarDriveIconUrl[] =
    "https://drive-thirdparty.googleusercontent.com/16/type/application/"
    "vnd.google-apps.document";

const char kOutlookCalendarDocIconUrl[] =
    "https://res.cdn.office.net/files/fabric-cdn-prod_20240925.001/assets/"
    "item-types/16/docx.png";

}  // namespace

namespace calendar::calendar_fake_data_helper {

ntp::calendar::mojom::CalendarEventPtr GetFakeEvent(
    int index,
    CalendarType calendar_type,
    bool has_attachments_enabled) {
  ntp::calendar::mojom::CalendarEventPtr event =
      ntp::calendar::mojom::CalendarEvent::New();
  event->title = "Calendar Event " + base::NumberToString(index);
  event->start_time = base::Time::Now() + base::Minutes(index * 30);
  event->end_time = event->start_time + base::Minutes(30);
  event->url = GURL("https://foo.com/" + base::NumberToString(index));
  event->location = "Conference Room " + base::NumberToString(index);
  for (int i = 0; i < 3; ++i) {
    ntp::calendar::mojom::AttachmentPtr attachment =
        ntp::calendar::mojom::Attachment::New();
    attachment->title = "Attachment " + base::NumberToString(i);
    if (has_attachments_enabled) {
      attachment->resource_url =
          GURL("https://foo.com/attachment" + base::NumberToString(i));
    }
    attachment->icon_url = calendar_type == CalendarType::GOOGLE_CALENDAR
                               ? GURL(kGoogleCalendarDriveIconUrl)
                               : GURL(kOutlookCalendarDocIconUrl);
    event->attachments.push_back(std::move(attachment));
  }
  event->conference_url =
      GURL("https://foo.com/conference" + base::NumberToString(index));
  event->is_accepted = true;
  event->has_other_attendee = false;
  return event;
}

std::vector<ntp::calendar::mojom::CalendarEventPtr> GetFakeEvents(
    CalendarType calendar_type,
    bool has_attachments_enabled) {
  std::vector<ntp::calendar::mojom::CalendarEventPtr> events;
  for (int i = 0; i < 5; ++i) {
    events.push_back(GetFakeEvent(i, calendar_type, has_attachments_enabled));
  }
  return events;
}

std::unique_ptr<std::string> GetFakeJsonResponse() {
  std::unique_ptr<std::string> response = std::make_unique<std::string>();
  // clang-format off
  *response = R"(
    {"data-context": "some-context",
    "value": [
      {
        "id": "1",
        "hasAttachments": true,
        "subject": "Event 1",
        "isCancelled": false,
        "isOrganizer": true,
        "webLink": "https://outlook.com",
        "responseStatus": {
            "response": "organizer",
            "time": "0001-01-01T00:00:00Z"
        },
        "onlineMeeting": {"joinUrl": "https://outlook.com"},
        "start": {"dateTime": "2024-11-11T18:00:00.0000000"},
        "end": {"dateTime": "2024-11-11T18:30:00.0000000"},
        "location": {"displayName": "Location Name"},
        "attendees": [
              {
                  "type": "required",
                  "status": {
                        "response": "none",
                        "time": "0001-01-01T00:00:00Z"
                  },
                  "emailAddress": {
                        "name": "test1@outlook.com",
                        "address": "test1@outlook.com"
                  }
              },
              {
                  "type": "required",
                  "status": {
                      "response": "accepted",
                      "time": "0001-01-01T00:00:00Z"
                  },
                  "emailAddress": {
                      "name": "test2@outlook.com",
                      "address": "test2@outlook.com"
                  }
              }
        ],
        "attachments": [
              {
                    "@odata.type": "#microsoft.graph.fileAttachment",
                    "@odata.mediaContentType":
                      "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                    "contentType":
                      "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                    "id": "1-ABC",
                    "name": "Some document.docx"
              }
        ]
      },
      {
        "id": "2",
        "hasAttachments": false,
        "subject": "Event 2",
        "isCancelled": false,
        "isOrganizer": true,
        "webLink": "https://outlook.com",
        "responseStatus": {
            "response": "organizer",
            "time": "0001-01-01T00:00:00Z"
        },
        "onlineMeeting": {"joinUrl": "https://outlook.com"},
        "start": {"dateTime": "2024-11-11T18:30:00.0000000"},
        "end": {"dateTime": "2024-11-11T19:00:00.0000000"},
        "location": {"displayName": "Location Name"},
        "attendees": [],
        "attachments": []
      },
      {
        "id": "3",
        "hasAttachments": false,
        "subject": "Event 3",
        "isCancelled": false,
        "isOrganizer": true,
        "webLink": "https://outlook.com",
        "responseStatus": {
            "response": "organizer",
            "time": "0001-01-01T00:00:00Z"
        },
        "onlineMeeting": {"joinUrl": "https://outlook.com"},
        "start": {"dateTime": "2024-11-11T20:00:00.0000000"},
        "end": {"dateTime": "2024-11-11T21:00:00.0000000"},
        "location": {"displayName": ""},
        "attendees": [],
        "attachments": []
      }]})";
  // clang-format on
  return response;
}

}  // namespace calendar::calendar_fake_data_helper
