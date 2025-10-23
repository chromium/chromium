// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_context_controller.h"
#include "net/base/url_util.h"
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
  explicit MockUiServiceForUrlIntercept(
      ContextualTasksContextController* context_controller)
      : ContextualTasksUiService(context_controller) {}
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
    context_controller_ =
        std::make_unique<MockContextualTasksContextController>();
    service_for_nav_ = std::make_unique<MockUiServiceForUrlIntercept>(
        context_controller_.get());
  }

  void TearDown() override {
    service_for_nav_ = nullptr;
    context_controller_ = nullptr;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockUiServiceForUrlIntercept> service_for_nav_;
  std::unique_ptr<MockContextualTasksContextController> context_controller_;
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
  task_environment_.RunUntilIdle();
}

// Ensure we're not intercepting a link when it doesn't meet any of our
// conditions.
TEST_F(ContextualTasksUiServiceTest, NormalLinkNotIntercepted) {
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      GURL(kTestUrl), GURL("https://example.com/foo"), nullptr, false));
  task_environment_.RunUntilIdle();
}

TEST_F(ContextualTasksUiServiceTest, AiHostNotIntercepted_BadPath) {
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      GURL(kTestUrl), GURL("https://google.com/maps?udm=50"), nullptr, false));
  task_environment_.RunUntilIdle();
}

TEST_F(ContextualTasksUiServiceTest, AiPageIntercepted_FromTab) {
  GURL ai_url(kAiPageUrl);
  GURL tab_url(kTestUrl);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .Times(1);
  EXPECT_TRUE(
      service_for_nav_->HandleNavigation(ai_url, tab_url, nullptr, false));
  task_environment_.RunUntilIdle();
}

TEST_F(ContextualTasksUiServiceTest, AiPageIntercepted_FromOmnibox) {
  GURL ai_url(kAiPageUrl);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .Times(1);
  EXPECT_TRUE(
      service_for_nav_->HandleNavigation(ai_url, GURL(), nullptr, false));
  task_environment_.RunUntilIdle();
}

// The AI page is allowed to load as long as it is part of the WebUI.
TEST_F(ContextualTasksUiServiceTest, AiPageNotIntercepted) {
  std::string webui_url = "chrome://" + std::string(kContextualTasksUiHost);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      GURL(kAiPageUrl), GURL(webui_url), nullptr, false));
  task_environment_.RunUntilIdle();
}

TEST_F(ContextualTasksUiServiceTest, ContextControllerUpdatedOnUrlChange) {
  GURL updated_url(kAiPageUrl);

  std::string turn_id = "1234";
  updated_url = net::AppendQueryParameter(updated_url, "mstk", turn_id);

  std::string thread_id = "5678";
  updated_url = net::AppendQueryParameter(updated_url, "mtid", thread_id);

  base::Uuid task_id =
      base::Uuid::ParseCaseInsensitive("10000000-0000-0000-0000-000000000000");
  std::string title = "title";

  EXPECT_CALL(
      *context_controller_,
      UpdateThreadForTask(task_id, _, thread_id, testing::Optional(turn_id),
                          testing::Optional(title)))
      .Times(1);

  service_for_nav_->OnWebUiInnerFrameNavigation(task_id, updated_url, title);
}

TEST_F(ContextualTasksUiServiceTest,
       ContextControllerUpdatedOnUrlChange_NoThreadId) {
  GURL updated_url(kAiPageUrl);

  std::string turn_id = "1234";
  updated_url = net::AppendQueryParameter(updated_url, "mstk", turn_id);

  base::Uuid task_id =
      base::Uuid::ParseCaseInsensitive("10000000-0000-0000-0000-000000000000");
  std::string title = "title";

  EXPECT_CALL(*context_controller_, UpdateThreadForTask(_, _, _, _, _))
      .Times(0);

  service_for_nav_->OnWebUiInnerFrameNavigation(task_id, updated_url, title);
}

// The task should still updated without a turn ID.
TEST_F(ContextualTasksUiServiceTest,
       ContextControllerUpdatedOnUrlChange_NoTurnId) {
  GURL updated_url(kAiPageUrl);

  std::string thread_id = "5678";
  updated_url = net::AppendQueryParameter(updated_url, "mtid", thread_id);

  base::Uuid task_id =
      base::Uuid::ParseCaseInsensitive("10000000-0000-0000-0000-000000000000");
  std::string title = "title";

  EXPECT_CALL(
      *context_controller_,
      UpdateThreadForTask(task_id, _, thread_id, _, testing::Optional(title)))
      .Times(1);

  service_for_nav_->OnWebUiInnerFrameNavigation(task_id, updated_url, title);
}

}  // namespace contextual_tasks
