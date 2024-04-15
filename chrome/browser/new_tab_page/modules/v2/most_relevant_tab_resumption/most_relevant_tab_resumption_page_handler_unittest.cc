// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption_page_handler.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/mojom/history_types.mojom.h"
#include "components/search/ntp_features.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MostRelevantTabResumptionPageHandlerTest
    : public BrowserWithTestWindowTest {
 public:
  MostRelevantTabResumptionPageHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    handler_ = std::make_unique<MostRelevantTabResumptionPageHandler>(
        mojo::PendingReceiver<
            ntp::most_relevant_tab_resumption::mojom::PageHandler>(),
        web_contents_.get());
  }

  void SetUpMockCalls(
      std::vector<history::mojom::TabPtr>& tabs_mojom,
      base::MockCallback<MostRelevantTabResumptionPageHandler::GetTabsCallback>&
          callback) {
    EXPECT_CALL(callback, Run(testing::_))
        .Times(1)
        .WillOnce(testing::Invoke(
            [&tabs_mojom](std::vector<history::mojom::TabPtr> tabs_arg) {
              tabs_mojom = std::move(tabs_arg);
            }));
  }

  void TearDown() override {
    handler_.reset();
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  MostRelevantTabResumptionPageHandler& handler() { return *handler_; }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<MostRelevantTabResumptionPageHandler> handler_;
};

TEST_F(MostRelevantTabResumptionPageHandlerTest, GetFakeTabs) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpMostRelevantTabResumptionModule,
           {{ntp_features::kNtpMostRelevantTabResumptionModuleDataParam,
             "Fake Data"}}},
      },
      {});
  std::vector<history::mojom::TabPtr> tabs_mojom;
  base::MockCallback<MostRelevantTabResumptionPageHandler::GetTabsCallback>
      callback;

  SetUpMockCalls(tabs_mojom, callback);

  handler().GetTabs(callback.Get());

  ASSERT_EQ(3u, tabs_mojom.size());

  for (const auto& tab_mojom : tabs_mojom) {
    ASSERT_EQ("Test Session", tab_mojom->session_name);
    ASSERT_EQ("5 mins ago", tab_mojom->relative_time_text);
    ASSERT_EQ(GURL("https://www.google.com"), tab_mojom->url);
  }
}
}  // namespace
