// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar_page_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/test_util.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kGoogleCalendarLastDismissedTimePrefName[] =
    "NewTabPage.GoogleCalendar.LastDimissedTime";

// Handles an HTTP request by returning a response from a json file in
// google_apis/test/data/.
std::unique_ptr<net::test_server::BasicHttpResponse> HandleRequest(
    const std::string& json_path) {
  return google_apis::test_util::CreateHttpResponseFromFile(
      google_apis::test_util::GetTestFilePath(json_path));
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
    pref_service_ = profile_->GetPrefs();
  }

  std::unique_ptr<GoogleCalendarPageHandler> CreateHandler() {
    return std::make_unique<GoogleCalendarPageHandler>(
        mojo::PendingReceiver<
            ntp::calendar::mojom::GoogleCalendarPageHandler>(),
        &profile());
  }

  std::unique_ptr<GoogleCalendarPageHandler> CreateHandlerWithInterceptor(
      std::string json_path) {
    google_apis::calendar::CalendarApiUrlGenerator url_generator;
    url_generator.SetBaseUrlForTesting("https://foo.com/");
    std::vector<google_apis::calendar::EventType> event_types = {
        google_apis::calendar::EventType::kDefault};
    SetUpEventsResponse(
        url_generator.GetCalendarEventListUrl(
            "primary", base::Time::Now(), base::Time::Now() + base::Hours(12),
            true, 1, 2500, event_types, "ntp-calendar", "startTime"),
        json_path);
    return std::make_unique<GoogleCalendarPageHandler>(
        mojo::PendingReceiver<
            ntp::calendar::mojom::GoogleCalendarPageHandler>(),
        profile_.get(), MakeRequestSender(), std::move(url_generator));
  }

  void SetUpEventsResponse(GURL request_url, std::string json_path) {
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
        HandleRequest(json_path)->content());
  }

  PrefService& pref_service() { return *pref_service_; }
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
  raw_ptr<PrefService> pref_service_;
};

TEST_F(GoogleCalendarPageHandlerTest, DismissAndRestoreModule) {
  std::unique_ptr<GoogleCalendarPageHandler> handler = CreateHandler();
  EXPECT_EQ(pref_service().GetTime(kGoogleCalendarLastDismissedTimePrefName),
            base::Time());
  handler->DismissModule();

  EXPECT_EQ(pref_service().GetTime(kGoogleCalendarLastDismissedTimePrefName),
            base::Time::Now());
  handler->RestoreModule();

  EXPECT_EQ(pref_service().GetTime(kGoogleCalendarLastDismissedTimePrefName),
            base::Time());
}

TEST_F(GoogleCalendarPageHandlerTest, DismissModuleAffectsEvents) {
  std::unique_ptr<GoogleCalendarPageHandler> handler = CreateHandler();
  base::FieldTrialParams params;
  params[ntp_features::kNtpCalendarModuleDataParam] = "fake";
  feature_list().Reset();
  feature_list().InitAndEnableFeatureWithParameters(
      ntp_features::kNtpCalendarModule, params);

  std::vector<ntp::calendar::mojom::CalendarEventPtr> response1;
  std::vector<ntp::calendar::mojom::CalendarEventPtr> response2;
  base::MockCallback<GoogleCalendarPageHandler::GetEventsCallback> callback1;
  base::MockCallback<GoogleCalendarPageHandler::GetEventsCallback> callback2;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](std::vector<ntp::calendar::mojom::CalendarEventPtr> events) {
            response1 = std::move(events);
          }));
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](std::vector<ntp::calendar::mojom::CalendarEventPtr> events) {
            response2 = std::move(events);
          }));

  handler->DismissModule();

  // Move time forward 1 hour.
  task_environment().AdvanceClock(base::Hours(1));

  // Expect empty result since it has been less than 12 hours.
  handler->GetEvents(callback1.Get());
  EXPECT_EQ(response1.size(), 0u);

  // Move clock forward 11 more hours to be at 12 hours since dismissal.
  task_environment().AdvanceClock(base::Hours(11));

  // Expect non-empty result since it has been 12 hours.
  handler->GetEvents(callback2.Get());
  EXPECT_GT(response2.size(), 0u);
}

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
      .WillOnce(testing::Invoke(
          [&](std::vector<ntp::calendar::mojom::CalendarEventPtr> events) {
            response = std::move(events);
          }));

  handler->GetEvents(callback.Get());
  EXPECT_EQ(response.size(), 5u);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(response[i]->title, "Calendar Event " + base::NumberToString(i));
    EXPECT_EQ(response[i]->start_time,
              base::Time::Now() + base::Minutes(i * 30));
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
  }
}

TEST_F(GoogleCalendarPageHandlerTest, GetEvents) {
  std::unique_ptr<GoogleCalendarPageHandler> handler =
      CreateHandlerWithInterceptor("calendar/events.json");
  std::vector<ntp::calendar::mojom::CalendarEventPtr> response;
  base::MockCallback<GoogleCalendarPageHandler::GetEventsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](std::vector<ntp::calendar::mojom::CalendarEventPtr> events) {
            response = std::move(events);
          }));

  base::RunLoop run_loop;
  handler->GetEvents(
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  EXPECT_EQ(response.size(), 3u);
  EXPECT_EQ(response[0]->title, "Mobile weekly team meeting ");
  base::Time start_time;
  bool success =
      base::Time::FromString("2020-11-02T10:00:00-08:00", &start_time);
  ASSERT_TRUE(success);
  EXPECT_EQ(response[0]->start_time, start_time);
  EXPECT_EQ(
      response[0]->url.spec(),
      "https://www.google.com/calendar/event?eid=b3I4MjIxc2lydDRvZ2Ztest");
  ASSERT_TRUE(response[0]->conference_url);
  EXPECT_EQ(response[0]->conference_url->spec(),
            "https://meet.google.com/jbe-test");
}

TEST_F(GoogleCalendarPageHandlerTest, GetEventWithAttachments) {
  std::unique_ptr<GoogleCalendarPageHandler> handler =
      CreateHandlerWithInterceptor("calendar/event_with_attachments.json");
  std::vector<ntp::calendar::mojom::CalendarEventPtr> response;
  base::MockCallback<GoogleCalendarPageHandler::GetEventsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](std::vector<ntp::calendar::mojom::CalendarEventPtr> events) {
            response = std::move(events);
          }));

  base::RunLoop run_loop;
  handler->GetEvents(
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  EXPECT_EQ(response.size(), 1u);
  EXPECT_EQ(response[0]->attachments.size(), 2u);
  EXPECT_EQ(response[0]->attachments[0]->title, "Google Docs Attachment");
  EXPECT_EQ(response[0]->attachments[0]->icon_url,
            "https://www.gstatic.com/images/branding/product/1x/"
            "docs_2020q4_48dp.png");
  EXPECT_EQ(response[0]->attachments[0]->resource_url,
            "https://docs.google.com/document/d/"
            "1yeRZ9Je4i9XvbnnOygitkXgJQpLvR98_TrfWRec84Bw/"
            "edit?tab=t.0&resourcekey=0-yNQRr67lHMYKNFyrXmvwBw");
}
