// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_context_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;

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
      : ContextualTasksUiService(nullptr, context_controller) {}
  ~MockUiServiceForUrlIntercept() override = default;

  MOCK_METHOD(void,
              OnNavigationToAiPageIntercepted,
              (const GURL& url,
               base::WeakPtr<tabs::TabInterface> tab,
               bool is_to_new_tab),
              (override));
  MOCK_METHOD(void,
              OnThreadLinkClicked,
              (const GURL& url,
               base::Uuid task_id,
               base::WeakPtr<tabs::TabInterface> tab),
              (override));
};

}  // namespace

class ContextualTasksUiServiceTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    context_controller_ =
        std::make_unique<MockContextualTasksContextController>();
    service_for_nav_ = std::make_unique<MockUiServiceForUrlIntercept>(
        context_controller_.get());
  }

  void TearDown() override {
    service_for_nav_ = nullptr;
    context_controller_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

 protected:
  std::unique_ptr<MockUiServiceForUrlIntercept> service_for_nav_;
  std::unique_ptr<MockContextualTasksContextController> context_controller_;
};

TEST_F(ContextualTasksUiServiceTest, IsAiUrl_InvalidUrl) {
  GURL url("http://?a=12345");
  EXPECT_FALSE(url.is_valid());
  EXPECT_FALSE(service_for_nav_->IsAiUrl(url));
}

TEST_F(ContextualTasksUiServiceTest, LinkFromWebUiIntercepted) {
  GURL navigated_url(kTestUrl);
  GURL host_web_content_url(chrome::kChromeUIContextualTasksURL);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(navigated_url, _, _))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      navigated_url, host_web_content_url, content::FrameTreeNodeId(), false));
  task_environment()->RunUntilIdle();
}

// Ensure we're not intercepting a link when it doesn't meet any of our
// conditions.
TEST_F(ContextualTasksUiServiceTest, NormalLinkNotIntercepted) {
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      GURL(kTestUrl), GURL("https://example.com/foo"),
      content::FrameTreeNodeId(), false));
  task_environment()->RunUntilIdle();
}

TEST_F(ContextualTasksUiServiceTest, AiHostNotIntercepted_BadPath) {
  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      GURL(kTestUrl), GURL("https://google.com/maps?udm=50"),
      content::FrameTreeNodeId(), false));
  task_environment()->RunUntilIdle();
}

TEST_F(ContextualTasksUiServiceTest, AiPageIntercepted_FromTab) {
  GURL ai_url(kAiPageUrl);
  GURL tab_url(kTestUrl);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .Times(1);
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      ai_url, tab_url, content::FrameTreeNodeId(), false));
  task_environment()->RunUntilIdle();
}

TEST_F(ContextualTasksUiServiceTest, AiPageIntercepted_FromOmnibox) {
  GURL ai_url(kAiPageUrl);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(ai_url, _, _))
      .Times(1);
  EXPECT_TRUE(service_for_nav_->HandleNavigation(
      ai_url, GURL(), content::FrameTreeNodeId(), false));
  task_environment()->RunUntilIdle();
}

// The AI page is allowed to load as long as it is part of the WebUI.
TEST_F(ContextualTasksUiServiceTest, AiPageNotIntercepted) {
  GURL webui_url(chrome::kChromeUIContextualTasksURL);

  EXPECT_CALL(*service_for_nav_, OnThreadLinkClicked(_, _, _)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnNavigationToAiPageIntercepted(_, _, _))
      .Times(0);
  EXPECT_FALSE(service_for_nav_->HandleNavigation(
      GURL(kAiPageUrl), webui_url, content::FrameTreeNodeId(), false));
  task_environment()->RunUntilIdle();
}

TEST_F(ContextualTasksUiServiceTest, OnNavigationToAiPageIntercepted_SameTab) {
  ContextualTasksUiService service(nullptr, context_controller_.get());
  GURL intercepted_url("https://google.com/search?udm=50&q=test+query");

  TestingProfile profile;
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      &profile, content::SiteInstance::Create(&profile));
  sessions::SessionTabHelper::CreateForWebContents(
      web_contents.get(),
      base::BindRepeating([](content::WebContents* contents) {
        return static_cast<sessions::SessionTabHelperDelegate*>(nullptr);
      }));

  tabs::MockTabInterface tab;
  ON_CALL(tab, GetContents).WillByDefault(Return(web_contents.get()));

  ContextualTask task(base::Uuid::GenerateRandomV4());
  EXPECT_CALL(*context_controller_, CreateTaskFromUrl(intercepted_url))
      .WillOnce(Return(task));
  EXPECT_CALL(*context_controller_,
              AssociateTabWithTask(
                  task.GetTaskId(),
                  sessions::SessionTabHelper::IdForTab(web_contents.get())))
      .Times(1);
  base::WeakPtrFactory weak_factory(&tab);

  service.OnNavigationToAiPageIntercepted(intercepted_url,
                                          weak_factory.GetWeakPtr(), false);

  GURL expected_initial_url(
      "https://www.google.com/search?udm=50&gsc=2&gl=us&q=test+query");
  EXPECT_EQ(service.GetInitialUrlForTask(task.GetTaskId()),
            expected_initial_url);
}

}  // namespace contextual_tasks
