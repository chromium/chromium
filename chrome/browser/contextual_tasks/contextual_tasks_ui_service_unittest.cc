// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

namespace {

constexpr char kTestUrl[] = "https://example.com";
constexpr char kAiPageUrl[] = "https://google.com/search?udm=50";

// A mock ContextualTasksUiService that is specifically used for tests around
// intercepting navigation. Namely the `HandleNavigation` method is the real
// implementation with the events being mocked.
class MockUiServiceForUrlIntercept : public ContextualTasksUiService {
 public:
  MockUiServiceForUrlIntercept() = default;
  ~MockUiServiceForUrlIntercept() override = default;

  MOCK_METHOD(void,
              OnNavigationToAiPageIntercepted,
              (const GURL& url,
               content::WebContents* source_contents,
               bool is_to_new_tab),
              (override));
  MOCK_METHOD(void,
              OnThreadLinkClicked,
              (const GURL& url, content::WebContents* source_contents),
              (override));
};

}  // namespace

class ContextualTasksUiServiceTest : public testing::Test {
 public:
  void SetUp() override {
    service_for_nav_ = std::make_unique<MockUiServiceForUrlIntercept>();
  }

 protected:
  std::unique_ptr<MockUiServiceForUrlIntercept> service_for_nav_;
};

TEST_F(ContextualTasksUiServiceTest, LinkFromWebUiIntercepted) {
  std::string webui_url = "chrome://" + std::string(kContextualTasksUiHost);
  GURL navigated_url(kTestUrl);
  GURL host_web_content_url("chrome://" + std::string(kContextualTasksUiHost));

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(navigated_url, _))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      navigated_url, host_web_content_url, nullptr, false));
}

// Ensure we're not intercepting a link when it doesn't meet any of our
// conditions.
TEST_F(ContextualTasksUiServiceTest, NormalLinkNotIntercepted) {
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      GURL(kTestUrl), GURL("https://example.com/foo"), nullptr, false));
}

TEST_F(ContextualTasksUiServiceTest, AiHostNotIntercepted_BadPath) {
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      GURL(kTestUrl), GURL("https://google.com/maps?udm=50"), nullptr, false));
}

TEST_F(ContextualTasksUiServiceTest, AiPageIntercepted_FromTab) {
  GURL ai_url(kAiPageUrl);
  GURL tab_url(kTestUrl);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .Times(1);
  EXPECT_TRUE(
      service_for_nav_->HandleNavigation(ai_url, tab_url, nullptr, false));
}

TEST_F(ContextualTasksUiServiceTest, AiPageIntercepted_FromOmnibox) {
  GURL ai_url(kAiPageUrl);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .Times(1);
  EXPECT_TRUE(
      service_for_nav_->HandleNavigation(ai_url, GURL(), nullptr, false));
}

// The AI page is allowed to load as long as it is part of the WebUI.
TEST_F(ContextualTasksUiServiceTest, AiPageNotIntercepted) {
  std::string webui_url = "chrome://" + std::string(kContextualTasksUiHost);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      GURL(kAiPageUrl), GURL(webui_url), nullptr, false));
}

}  // namespace contextual_tasks
