// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/authentication/microsoft_auth_page_handler.h"

#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MicrosoftAuthPageHandlerTest : public testing::Test {
 public:
  MicrosoftAuthPageHandlerTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpMicrosoftAuthenticationModule},
        /*disabled_features=*/{});
    profile_ = std::make_unique<TestingProfile>();
    pref_service_ = profile_->GetPrefs();
  }

  void SetUp() override {
    handler_ = std::make_unique<MicrosoftAuthPageHandler>(
        mojo::PendingReceiver<
            ntp::authentication::mojom::MicrosoftAuthPageHandler>(),
        profile_.get());
  }

  void TearDown() override { handler_.reset(); }

  MicrosoftAuthPageHandler& handler() { return *handler_; }
  PrefService& pref_service() { return *pref_service_; }
  TestingProfile& profile() { return *profile_; }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MicrosoftAuthPageHandler> handler_;
  raw_ptr<PrefService> pref_service_;
};

TEST_F(MicrosoftAuthPageHandlerTest, DismissAndRestoreModule) {
  ASSERT_EQ(pref_service().GetTime(
                MicrosoftAuthPageHandler::kLastDismissedTimePrefName),
            base::Time());

  handler().DismissModule();

  EXPECT_EQ(pref_service().GetTime(
                MicrosoftAuthPageHandler::kLastDismissedTimePrefName),
            base::Time::Now());

  handler().RestoreModule();

  EXPECT_EQ(pref_service().GetTime(
                MicrosoftAuthPageHandler::kLastDismissedTimePrefName),
            base::Time());
}

class MicrosoftAuthPageHandlerShowModuleTest
    : public MicrosoftAuthPageHandlerTest,
      public ::testing::WithParamInterface<bool> {
 public:
  bool ShouldShow() const { return GetParam(); }
};

TEST_P(MicrosoftAuthPageHandlerShowModuleTest, ShouldShowModule_False) {
  handler().DismissModule();

  base::MockCallback<ntp::authentication::mojom::MicrosoftAuthPageHandler::
                         ShouldShowModuleCallback>
      should_show_module_callback;
  bool show;
  EXPECT_CALL(should_show_module_callback, Run(testing::_))
      .Times(1)
      .WillOnce([&show](bool show_arg) { show = std::move(show_arg); });

  if (ShouldShow()) {
    task_environment().AdvanceClock(MicrosoftAuthPageHandler::kDismissDuration);
  }
  handler().ShouldShowModule(should_show_module_callback.Get());

  EXPECT_EQ(ShouldShow(), show);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MicrosoftAuthPageHandlerShowModuleTest,
                         ::testing::Bool());
