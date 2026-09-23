// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar_page_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/hash/hash.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "chrome/browser/new_tab_page/modules/modules_constants.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_data.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int32_t kNumEvents = 10;

base::ListValue CreateAttachments() {
  base::ListValue attachments = base::ListValue();
  for (int i = 0; i < 2; i++) {
    base::DictValue attachment =
        base::DictValue()
            .Set("fileUrl", "https://foo-file.com/" + base::NumberToString(i))
            .Set("title", "Test File " + base::NumberToString(i))
            .Set("iconLink", "https://foo-icon.com/" + base::NumberToString(i));
    attachments.Append(std::move(attachment));
  }
  return attachments;
}

base::ListValue CreateAttendees(int index) {
  std::string self_status = index % 2 == 0 ? "accepted" : "needsAction";
  if (index == 1) {
    self_status = "declined";
  }
  return base::ListValue()
      .Append(base::DictValue()
                  .Set("email", "test@test.com")
                  .Set("displayName", "Foo Test")
                  .Set("self", true)
                  .Set("responseStatus", self_status))
      .Append(
          base::DictValue()
              .Set("email", "test@test2.com")
              .Set("displayName", "Bar Test")
              .Set("responseStatus", index % 2 == 0 ? "accepted" : "declined"));
}

base::DictValue CreateConferenceData() {
  base::DictValue entryPoint =
      base::DictValue()
          .Set("entryPointType", "video")
          .Set("uri", "https://meet.google.com/jbe-test")
          .Set("label", "meet.google.com/jbe-test");
  return base::DictValue().Set("entryPoints",
                               base::ListValue().Append(std::move(entryPoint)));
}

base::DictValue CreateEventTime(bool is_all_day_event, bool is_end_time) {
  base::DictValue eventTime =
      base::DictValue()
          .Set("dateTime", is_end_time ? "2020-11-02T10:30:00-08:00"
                                       : "2020-11-02T10:00:00-08:00")
          .Set("timeZone", "America/Los_Angeles");
  if (is_all_day_event) {
    eventTime.Set("date", "2020-11-02");
  }
  return eventTime;
}

base::DictValue CreateEvent(int index) {
  return base::DictValue()
      .Set("kind", "calendar#event")
      .Set("status", "confirmed")
      .Set("htmlLink", "https://foo.com/" + base::NumberToString(index))
      .Set("created", "2018-05-14T18:55:59.000Z")
      .Set("updated", "2021-03-17T10:42:53.637Z")
      .Set("summary", "Test Event " + base::NumberToString(index))
      // Make 2nd event an all day event.
      .Set("start", CreateEventTime(/*is_all_day_event*/ index == 0,
                                    /*is_end_time=*/false))
      .Set("end", CreateEventTime(/*is_all_day_event*/ index == 0,
                                  /*is_end_time*/ true))
      .Set("conferenceData", CreateConferenceData())
      .Set("attachments", CreateAttachments())
      .Set("attendees", CreateAttendees(index));
}

bool CreateEventsJson(std::string* json) {
  base::ListValue events = base::ListValue();
  for (int i = 0; i < kNumEvents; i++) {
    events.Append(CreateEvent(i));
  }
  base::DictValue result_dict = base::DictValue()
                                    .Set("kind", "calendar#events")
                                    .Set("etag", "\"p32ofplf5q6gf20g\"")
                                    .Set("summary", "test1@google.com")
                                    .Set("updated", "2021-06-18T07:17:10.718Z")
                                    .Set("timeZone", "America/Los_Angeles")
                                    .Set("accessRole", "owner")
                                    .Set("items", std::move(events));
  JSONStringValueSerializer serializer(json);
  return serializer.Serialize(result_dict);
}

std::unique_ptr<TestingProfile> MakeTestingProfile(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetSharedURLLoaderFactory(url_loader_factory);
  return profile_builder.Build();
  ;
}

}  // namespace

class GoogleCalendarPageHandlerTest : public testing::Test {
 public:
  GoogleCalendarPageHandlerTest() {
    feature_list_.InitAndEnableFeature(ntp_features::kNtpCalendarModule);
    profile_ =
        MakeTestingProfile(test_url_loader_factory_.GetSafeWeakWrapper());
  }

  std::unique_ptr<GoogleCalendarPageHandler> CreateHandler() {
    return std::make_unique<GoogleCalendarPageHandler>(
        mojo::PendingReceiver<
            ntp::calendar::mojom::GoogleCalendarPageHandler>(),
        &profile());
  }

  std::unique_ptr<GoogleCalendarPageHandler> CreateHandlerWithTestData(
      GURL request_url,
      const std::string& test_data) {
    SetUpEventsResponse(request_url, test_data);
    return std::make_unique<GoogleCalendarPageHandler>(
        mojo::PendingReceiver<
            ntp::calendar::mojom::GoogleCalendarPageHandler>(),
        profile_.get(), MakeRequestSender());
  }

  std::unique_ptr<GoogleCalendarPageHandler> CreateHandlerWithTestData(
      const std::string& test_data) {
    google_apis::calendar::CalendarApiUrlGenerator url_generator;
    std::vector<google_apis::calendar::EventType> event_types = {
        google_apis::calendar::EventType::kDefault};
    return CreateHandlerWithTestData(
        url_generator.GetCalendarEventListUrl(
            /*calendar_id=*/"primary",
            /*start_time=*/base::Time::Now() - base::Minutes(15),
            /*end_time=*/base::Time::Now() + base::Hours(12),
            /*single_events=*/true,
            /*max_attendees=*/std::nullopt,
            /*max_results=*/2500, event_types,
            /*experiment=*/"ntp-calendar",
            /*order_by=*/"startTime"),
        test_data);
  }

  void SetUpEventsResponse(GURL request_url, const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {}));
    test_url_loader_factory_.AddResponse(
        request_url.spec() +
            "&fields=timeZone%2Cetag%2Ckind%2Citems(id%2Ckind%2Csummary%2C"
            "colorId%2Cstatus%2Cstart(date)%2Cend(date)%2Cstart(dateTime)%2C"
            "end(dateTime)%2ChtmlLink%2Cattendees(responseStatus%2Cself)%2C"
            "attendeesOmitted%2CconferenceData(conferenceId%2C"
            "entryPoints(entryPointType%2Curi))%2Ccreator(self)%2Clocation%2C"
            "attachments(title%2CfileUrl%2CiconLink%2CfileId))",
        response);
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  TestingProfile& profile() { return *profile_; }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }
  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

 private:
  // Makes a request sender configured for testing.
  std::unique_ptr<google_apis::RequestSender> MakeRequestSender() {
    return std::make_unique<google_apis::RequestSender>(
        std::make_unique<google_apis::DummyAuthService>(),
        profile_->GetURLLoaderFactory(),
        task_environment_.GetMainThreadTaskRunner(), "test-user-agent",
        TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::IO};
  base::test::ScopedFeatureList feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  base::HistogramTester histogram_tester_;
};

TEST_F(GoogleCalendarPageHandlerTest, GetFakeEvents) {
  std::unique_ptr<GoogleCalendarPageHandler> handler = CreateHandler();
  base::FieldTrialParams params;
  params[ntp_features::kNtpCalendarModuleDataParam] = "fake";
  feature_list().Reset();
  feature_list().InitAndEnableFeatureWithParameters(
      ntp_features::kNtpCalendarModule, params);

  std::vector<ntp::calendar::mojom::CalendarEventPtr> response;
  base::MockCallback<GoogleCalendarPageHandler::GetEventsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          [&](std::vector<ntp::calendar::mojom::CalendarEventPtr> events) {
            response = std::move(events);
          });

  handler->GetEvents(callback.Get());
  EXPECT_EQ(response.size(), 5u);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(response[i]->title, "Calendar Event " + base::NumberToString(i));
    EXPECT_EQ(response[i]->start_time,
              base::Time::Now() + base::Minutes(i * 30));
    EXPECT_EQ(response[i]->end_time,
              response[i]->start_time + base::Minutes(30));
    EXPECT_EQ(response[i]->url,
              GURL("https://foo.com/" + base::NumberToString(i)));
    EXPECT_EQ(response[i]->attachments.size(), 3u);
    for (int j = 0; j < 3; ++j) {
      ntp::calendar::mojom::AttachmentPtr attachment =
          std::move(response[i]->attachments[j]);
      EXPECT_EQ(attachment->title, "Attachment " + base::NumberToString(j));
      EXPECT_EQ(attachment->resource_url,
                "https://foo.com/attachment" + base::NumberToString(j));
    }
    EXPECT_EQ(response[i]->conference_url,
              GURL("https://foo.com/conference" + base::NumberToString(i)));
    EXPECT_TRUE(response[i]->is_accepted);
    EXPECT_FALSE(response[i]->has_other_attendee);
  }
}

TEST_F(GoogleCalendarPageHandlerTest, GetEvents) {
  std::vector<ntp::calendar::mojom::CalendarEventPtr> response;
  base::MockCallback<GoogleCalendarPageHandler::GetEventsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          [&](std::vector<ntp::calendar::mojom::CalendarEventPtr> events) {
            response = std::move(events);
          });

  std::string json;
  bool data_success = CreateEventsJson(&json);
  ASSERT_TRUE(data_success);
  std::unique_ptr<GoogleCalendarPageHandler> handler =
      CreateHandlerWithTestData(std::move(json));

  base::RunLoop run_loop;
  handler->GetEvents(
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  // The test data has 10 events, but we never return more than 5.
  ASSERT_EQ(response.size(), 5u);
  // The first event was an all day event, and the second event was declined by
  // the user. They were both filtered out, so the rest of the events should be
  // two numbers higher in their fields.
  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(response[i]->title, "Test Event " + base::NumberToString(i + 2));
    base::Time start_time;
    bool success =
        base::Time::FromString("2020-11-02T10:00:00-08:00", &start_time);
    ASSERT_TRUE(success);
    EXPECT_EQ(response[i]->start_time, start_time);
    base::Time end_time;
    success = base::Time::FromString("2020-11-02T10:30:00-08:00", &end_time);
    ASSERT_TRUE(success);
    EXPECT_EQ(response[i]->end_time, end_time);
    EXPECT_EQ(response[i]->url.spec(),
              "https://foo.com/" + base::NumberToString(i + 2));
    ASSERT_TRUE(response[i]->conference_url);
    EXPECT_EQ(response[i]->conference_url->spec(),
              "https://meet.google.com/jbe-test");
    EXPECT_EQ(response[i]->is_accepted, (i + 2) % 2 == 0);
    EXPECT_EQ(response[i]->has_other_attendee, (i + 2) % 2 == 0);

    for (int j = 0; j < 2; j++) {
      ASSERT_EQ(response[i]->attachments.size(), 2u);
      EXPECT_EQ(response[i]->attachments[j]->resource_url,
                "https://foo-file.com/" + base::NumberToString(j));
      EXPECT_EQ(response[i]->attachments[j]->title,
                "Test File " + base::NumberToString(j));
      EXPECT_EQ(response[i]->attachments[j]->icon_url,
                "https://foo-icon.com/" + base::NumberToString(j));
    }
  }
  histogram_tester().ExpectBucketCount(
      "NewTabPage.GoogleCalendar.RequestResult", kNumEvents, 1);
  histogram_tester().ExpectBucketCount(
      "NewTabPage.Modules.DataRequest",
      base::PersistentHash(ntp_modules::kGoogleCalendarModuleId), 1);
}

TEST_F(GoogleCalendarPageHandlerTest, GetEventsWithFeatureParams) {
  base::FieldTrialParams params;
  params[ntp_features::kNtpCalendarModuleExperimentParam.name] =
      "test_experiment_param";
  params[ntp_features::kNtpCalendarModuleMaxEventsParam.name] = "3";
  params[ntp_features::kNtpCalendarModuleWindowEndDeltaParam.name] = "8h";
  params[ntp_features::kNtpCalendarModuleWindowStartDeltaParam.name] = "30m";
  feature_list().Reset();
  feature_list().InitAndEnableFeatureWithParameters(
      ntp_features::kNtpCalendarModule, params);

  std::vector<ntp::calendar::mojom::CalendarEventPtr> response;
  base::MockCallback<GoogleCalendarPageHandler::GetEventsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          [&](std::vector<ntp::calendar::mojom::CalendarEventPtr> events) {
            response = std::move(events);
          });

  // Setting the request url here tests that the feature params are working
  // properly. If they are not, the url loader won't intercept the call.
  // This has to match the same values passed by the handler.
  google_apis::calendar::CalendarApiUrlGenerator url_generator;
  std::vector<google_apis::calendar::EventType> event_types = {
      google_apis::calendar::EventType::kDefault};
  std::string json;
  bool data_success = CreateEventsJson(&json);
  ASSERT_TRUE(data_success);
  std::unique_ptr<GoogleCalendarPageHandler> handler =
      CreateHandlerWithTestData(
          url_generator.GetCalendarEventListUrl(
              /*calendar_id=*/"primary",
              /*start_time=*/base::Time::Now() + base::Minutes(30),
              /*end_time=*/base::Time::Now() + base::Hours(8),
              /*single_events=*/true,
              /*max_attendees*/ std::nullopt,
              /*max_results*/ 2500, event_types,
              /*experiment=*/"test_experiment_param",
              /*order_by=*/"startTime"),
          json);

  base::RunLoop run_loop;
  handler->GetEvents(
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  EXPECT_EQ(response.size(), 3u);
}

TEST_F(GoogleCalendarPageHandlerTest, GetEventsFiltersPrivilegedSchemes) {
  auto create_event = []() {
    return base::DictValue()
        .Set("kind", "calendar#event")
        .Set("status", "confirmed")
        .Set("htmlLink", "https://foo.com/valid")
        .Set("created", "2018-05-14T18:55:59.000Z")
        .Set("updated", "2021-03-17T10:42:53.637Z")
        .Set("summary", "Test Event")
        .Set("start", CreateEventTime(/*is_all_day_event=*/false,
                                      /*is_end_time=*/false))
        .Set("end", CreateEventTime(/*is_all_day_event=*/false,
                                    /*is_end_time=*/true))
        .Set("conferenceData", CreateConferenceData())
        .Set("attendees", CreateAttendees(0));
  };

  auto test_invalid_event = [this](base::DictValue event) {
    base::ListValue events;
    events.Append(std::move(event));

    base::DictValue result_dict =
        base::DictValue()
            .Set("kind", "calendar#events")
            .Set("etag", "\"p32ofplf5q6gf20g\"")
            .Set("summary", "test1@google.com")
            .Set("updated", "2021-06-18T07:17:10.718Z")
            .Set("timeZone", "America/Los_Angeles")
            .Set("accessRole", "owner")
            .Set("items", std::move(events));
    std::string json;
    JSONStringValueSerializer serializer(&json);
    ASSERT_TRUE(serializer.Serialize(result_dict));

    std::unique_ptr<GoogleCalendarPageHandler> handler =
        CreateHandlerWithTestData(std::move(json));

    std::vector<ntp::calendar::mojom::CalendarEventPtr> response;
    base::MockCallback<GoogleCalendarPageHandler::GetEventsCallback> callback;
    EXPECT_CALL(callback, Run(testing::_))
        .Times(1)
        .WillOnce(
            [&](std::vector<ntp::calendar::mojom::CalendarEventPtr> events) {
              response = std::move(events);
            });

    base::RunLoop run_loop;
    handler->GetEvents(
        google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
    run_loop.Run();

    // If any event or attachment has an invalid URL, the entire request fails
    // and returns an empty list so the module is not displayed.
    EXPECT_TRUE(response.empty());
  };

  // 1. Test invalid htmlLink.
  {
    base::DictValue event = create_event();
    event.Set("htmlLink", "chrome://settings");
    event.Set("attachments", CreateAttachments());
    test_invalid_event(std::move(event));
  }

  // 2. Test invalid attachment fileUrl (with a valid iconLink).
  {
    base::DictValue invalid_attachment =
        base::DictValue()
            .Set("fileUrl", "chrome://settings")
            .Set("title", "Attachment With Invalid File URL")
            .Set("iconLink", "https://foo-icon.com/valid");
    base::ListValue attachments;
    attachments.Append(std::move(invalid_attachment));

    base::DictValue event = create_event();
    event.Set("attachments", std::move(attachments));
    test_invalid_event(std::move(event));
  }

  // 3. Test invalid attachment iconLink (with a valid fileUrl).
  {
    base::DictValue invalid_attachment =
        base::DictValue()
            .Set("fileUrl", "https://foo-file.com/valid")
            .Set("title", "Attachment With Invalid Icon Link")
            .Set("iconLink", "chrome://image?url=https://attacker.example");
    base::ListValue attachments;
    attachments.Append(std::move(invalid_attachment));

    base::DictValue event = create_event();
    event.Set("attachments", std::move(attachments));
    test_invalid_event(std::move(event));
  }
}

