// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar_page_handler.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_data.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_fake_data_helper.h"
#include "components/search/ntp_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDocIconUrl[] =
    "https://res.cdn.office.net/files/fabric-cdn-prod_20240925.001/assets/"
    "item-types/16/docx.png";

const char kBaseAttachmentResourceUrl[] =
    "https://outlook.live.com/mail/0/deeplink/attachment/";

}  // namespace

class OutlookCalendarPageHandlerTest : public testing::Test {
 public:
  OutlookCalendarPageHandlerTest() = default;

  std::unique_ptr<OutlookCalendarPageHandler> CreateHandler() {
    return std::make_unique<OutlookCalendarPageHandler>(
        mojo::PendingReceiver<
            ntp::calendar::mojom::OutlookCalendarPageHandler>());
  }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(OutlookCalendarPageHandlerTest, GetFakeEvents) {
  // kNtpOutlookCalendarModuleDataParam must have a value to use fake events.
  feature_list().InitAndEnableFeatureWithParameters(
      ntp_features::kNtpOutlookCalendarModule,
      {{ntp_features::kNtpOutlookCalendarModuleDataParam, "fake"}});
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;

  handler->GetEvents(future.GetCallback());
  const std::vector<ntp::calendar::mojom::CalendarEventPtr>& events =
      future.Get();
  EXPECT_EQ(events.size(), 5u);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(events[i]->title, "Calendar Event " + base::NumberToString(i));
    EXPECT_EQ(events[i]->start_time, base::Time::Now() + base::Minutes(i * 30));
    EXPECT_EQ(events[i]->end_time, events[i]->start_time + base::Minutes(30));
    EXPECT_EQ(events[i]->url,
              GURL("https://foo.com/" + base::NumberToString(i)));
    EXPECT_EQ(events[i]->attachments.size(), 3u);
    for (int j = 0; j < 3; ++j) {
      ntp::calendar::mojom::AttachmentPtr attachment =
          std::move(events[i]->attachments[j]);
      EXPECT_EQ(attachment->title, "Attachment " + base::NumberToString(j));
      EXPECT_FALSE(attachment->resource_url.has_value());
    }
    EXPECT_EQ(events[i]->conference_url,
              GURL("https://foo.com/conference" + base::NumberToString(i)));
    EXPECT_TRUE(events[i]->is_accepted);
    EXPECT_FALSE(events[i]->has_other_attendee);
  }
}

TEST_F(OutlookCalendarPageHandlerTest, GetEvents) {
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;
  // For now, `GetEvents()` has fake calendar event data hardcoded.
  handler->GetEvents(future.GetCallback());
  EXPECT_EQ(future.Get().size(), 3u);
}

TEST_F(OutlookCalendarPageHandlerTest, EmptyResponse) {
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;

  handler->GetEventsForTesting(future.GetCallback(), "");
  EXPECT_EQ(future.Get().size(), 0u);
}

TEST_F(OutlookCalendarPageHandlerTest, MalformedResponse) {
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;

  handler->GetEventsForTesting(future.GetCallback(), "} {");
  EXPECT_EQ(future.Get().size(), 0u);
}

TEST_F(OutlookCalendarPageHandlerTest, ResponseMissingData) {
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;

  // Missing event title.
  std::string response = R"(
    {"data-context": "some-context",
    "value": [
      {
        "id": "1",
        "hasAttachments": false,
        "isCancelled": false,
        "isOrganizer": true,
        "responseStatus": {
            "response": "organizer",
            "time": "0001-01-01T00:00:00Z"
        },
        "webLink": "https://outlook.com",
        "onlineMeeting": {"joinUrl": "https://outlook.com"},
        "start": {"dateTime": "2024-11-11T18:00:00.0000000"},
        "end": {"dateTime": "2024-11-11T18:30:00.0000000"},
        "location": {"displayName": "Location Name"},
        "attendees": [],
        "attachments": []
      }]})";
  handler->GetEventsForTesting(future.GetCallback(), response);
  EXPECT_EQ(future.Get().size(), 0u);
}

TEST_F(OutlookCalendarPageHandlerTest, ResponsePropertyHasWrongDataType) {
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;

  // `hasAttachments` should be a boolean, not a string value.
  std::string response = R"(
    {"data-context": "some-context",
    "value": [
      {
        "id": "1",
        "hasAttachments": "false",
        "subject": "Event"
        "isCancelled": false,
        "isOrganizer": true,
        "responseStatus": {
            "response": "organizer",
            "time": "0001-01-01T00:00:00Z"
        },
        "webLink": "https://outlook.com",
        "onlineMeeting": {"joinUrl": "https://outlook.com"},
        "start": {"dateTime": "2024-11-11T18:00:00.0000000"},
        "end": {"dateTime": "2024-11-11T18:30:00.0000000"},
        "location": {"displayName": "Location Name"},
        "attendees": [],
        "attachments": []
      }]})";
  handler->GetEventsForTesting(future.GetCallback(), response);
  EXPECT_EQ(future.Get().size(), 0u);
}

TEST_F(OutlookCalendarPageHandlerTest, OptionalDataMissing) {
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;
  // Event conference URL does not exist.
  std::string response = R"(
    {"data-context": "some-context",
    "value": [
      {
        "id": "1",
        "hasAttachments": false,
        "subject": "Event",
        "isCancelled": false,
        "isOrganizer": true,
        "responseStatus": {
            "response": "organizer",
            "time": "0001-01-01T00:00:00Z"
        },
        "webLink": "https://outlook.com",
        "onlineMeeting": null,
        "start": {"dateTime": "2024-11-11T18:00:00.0000000"},
        "end": {"dateTime": "2024-11-11T18:30:00.0000000"},
        "location": {"displayName": "Location Name"},
        "attendees": [],
        "attachments": []
      }]})";

  handler->GetEventsForTesting(future.GetCallback(), response);
  EXPECT_EQ(future.Get().size(), 1u);
}

TEST_F(OutlookCalendarPageHandlerTest, HasOtherAttendeeWhenNotOrganizer) {
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;
  // When a user is not the organizer and the event is not canceled, by default
  // there is another attendee (organizer).
  std::string response = R"(
    {"data-context": "some-context",
    "value": [
      {
        "id": "1",
        "hasAttachments": false,
        "subject": "Event 1",
        "isCancelled": false,
        "isOrganizer": false,
        "webLink": "https://outlook.com",
        "responseStatus": {
                "response": "notResponded",
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
                        "name": "test@outlook.com",
                        "address": "test@outlook.com"
                  }
              },
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
              }
        ],
        "attachments": []
      }]})";

  handler->GetEventsForTesting(future.GetCallback(), response);
  const std::vector<ntp::calendar::mojom::CalendarEventPtr>& events =
      future.Get();
  EXPECT_EQ(events.size(), 1u);
  for (auto& event : events) {
    EXPECT_TRUE(event->has_other_attendee);
  }
}

TEST_F(OutlookCalendarPageHandlerTest, AttendeesAccepted) {
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;
  // User is the organizer. Other attendees have accepted event.
  std::string response = R"(
    {"data-context": "some-context",
    "value": [
      {
        "id": "1",
        "hasAttachments": false,
        "subject": "Event 1",
        "isCancelled": false,
        "isOrganizer": true,
        "webLink": "https://outlook.com",
        "responseStatus": {
                "response": "none",
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
        "attachments": []
      }]})";

  handler->GetEventsForTesting(future.GetCallback(), response);
  const std::vector<ntp::calendar::mojom::CalendarEventPtr>& events =
      future.Get();
  EXPECT_EQ(events.size(), 1u);
  for (auto& event : events) {
    EXPECT_TRUE(event->has_other_attendee);
  }
}

TEST_F(OutlookCalendarPageHandlerTest, AttendeesDeclined) {
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;
  // User is the organizer. No attendees have accepted event.
  std::string response = R"(
    {"data-context": "some-context",
    "value": [
      {
        "id": "1",
        "hasAttachments": false,
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
                        "response": "declined",
                        "time": "0001-01-01T00:00:00Z"
                  },
                  "emailAddress": {
                        "name": "test2@outlook.com",
                        "address": "test2@outlook.com"
                  }
              }
        ],
        "attachments": []
      }]})";

  handler->GetEventsForTesting(future.GetCallback(), response);
  const std::vector<ntp::calendar::mojom::CalendarEventPtr>& events =
      future.Get();
  EXPECT_EQ(events.size(), 1u);
  for (auto& event : events) {
    EXPECT_FALSE(event->has_other_attendee);
  }
}

TEST_F(OutlookCalendarPageHandlerTest, EventCanceled) {
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;

  // Event is canceled.
  std::string response = R"(
    {"data-context": "some-context",
    "value": [
      {
        "id": "1",
        "hasAttachments": false,
        "subject": "Canceled: Event 1",
        "isCancelled": true,
        "isOrganizer": false,
        "webLink": "https://outlook.com",
        "responseStatus": {
                "response": "notResponded",
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
                        "name": "test@outlook.com",
                        "address": "test@outlook.com"
                  }
              },
              {
                  "type": "required",
                  "status": {
                        "response": "none",
                        "time": "0001-01-01T00:00:00Z"
                  },
                  "emailAddress": {
                        "name": "tes1t@outlook.com",
                        "address": "test1@outlook.com"
                  }
              }
        ],
        "attachments": []
      }]})";

  handler->GetEventsForTesting(future.GetCallback(), response);
  const std::vector<ntp::calendar::mojom::CalendarEventPtr>& events =
      future.Get();
  EXPECT_EQ(events.size(), 1u);
  for (auto& event : events) {
    EXPECT_FALSE(event->has_other_attendee);
  }
}

TEST_F(OutlookCalendarPageHandlerTest, AttachmentCreation) {
  std::unique_ptr<OutlookCalendarPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<ntp::calendar::mojom::CalendarEventPtr>>
      future;

  std::string event_id = "1";
  std::string attachment_id_1 = event_id + "-ABC";
  std::string attachment_id_2 = event_id + "-DEF";
  std::string attachment_id_3 = event_id + "-XYZ";
  std::vector<std::string> id_paths = {event_id + "/" + attachment_id_1,
                                       event_id + "/" + attachment_id_2,
                                       event_id + "/" + attachment_id_3};

  // clang-format off
  std::string response = base::StringPrintf(R"(
    {"data-context": "some-context",
    "value": [
      {
        "id": "%s",
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
                    "id": "%s",
                    "name": "Attachment 0.docx"
              },
              {
                    "@odata.type": "#microsoft.graph.fileAttachment",
                    "@odata.mediaContentType":
                      "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                    "id": "%s",
                    "name": "Attachment 1.docx"
              },
              {
                    "@odata.type": "#microsoft.graph.fileAttachment",
                    "@odata.mediaContentType":
                      "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                    "id": "%s",
                    "name": "Attachment 2.docx"
              }
        ]
      }]})", event_id, attachment_id_1, attachment_id_2, attachment_id_3);
  // clang-format on

  handler->GetEventsForTesting(future.GetCallback(), response);
  const std::vector<ntp::calendar::mojom::CalendarEventPtr>& events =
      future.Get();
  EXPECT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0]->attachments.size(), 3u);

  for (int i = 0; i < 3; i++) {
    ntp::calendar::mojom::AttachmentPtr attachment =
        std::move(events[0]->attachments[i]);
    EXPECT_EQ(attachment->title, "Attachment " + base::NumberToString(i));
    EXPECT_EQ(attachment->icon_url, GURL(kDocIconUrl));
    EXPECT_EQ(attachment->resource_url,
              GURL(kBaseAttachmentResourceUrl + id_paths[i]));
  }
}
