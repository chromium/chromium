// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar_page_handler.h"

#include <string>
#include <vector>

#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kGoogleCalendarLastDismissedTimePrefName[] =
    "NewTabPage.GoogleCalendar.LastDimissedTime";

}  // namespace

class GoogleCalendarPageHandlerTest : public testing::Test {
 public:
  GoogleCalendarPageHandlerTest() {
    feature_list_.InitAndEnableFeature(ntp_features::kNtpCalendarModule);
    profile_ = std::make_unique<TestingProfile>();
    pref_service_ = profile_->GetPrefs();
  }

  void SetUp() override {
    handler_ = std::make_unique<GoogleCalendarPageHandler>(
        mojo::PendingReceiver<
            ntp::calendar::mojom::GoogleCalendarPageHandler>(),
        profile_.get());
  }

  void TearDown() override { handler_.reset(); }

  GoogleCalendarPageHandler& handler() { return *handler_; }
  PrefService& pref_service() { return *pref_service_; }
  TestingProfile& profile() { return *profile_; }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }
  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

 private:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<GoogleCalendarPageHandler> handler_;
  raw_ptr<PrefService> pref_service_;
};

TEST_F(GoogleCalendarPageHandlerTest, DismissAndRestoreModule) {
  EXPECT_EQ(pref_service().GetTime(kGoogleCalendarLastDismissedTimePrefName),
            base::Time());
  handler().DismissModule();

  EXPECT_EQ(pref_service().GetTime(kGoogleCalendarLastDismissedTimePrefName),
            base::Time::Now());
  handler().RestoreModule();

  EXPECT_EQ(pref_service().GetTime(kGoogleCalendarLastDismissedTimePrefName),
            base::Time());
}

TEST_F(GoogleCalendarPageHandlerTest, GetFakeEvents) {
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

  handler().GetEvents(callback.Get());
  EXPECT_EQ(response.size(), 3u);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(response[i]->title, "Calendar Event " + base::NumberToString(i));
  }
}
