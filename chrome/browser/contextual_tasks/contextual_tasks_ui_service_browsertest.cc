// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include <vector>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/companion/text_finder/text_highlighter_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_media_link_handler.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_media_session.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_tasks {

class ContextualTasksUiServiceBrowserTest : public InProcessBrowserTest {
 public:
  ContextualTasksUiServiceBrowserTest() {
    feature_list_.InitAndEnableFeature(contextual_tasks::kContextualTasks);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that opening a citation for a URL that is already open highlights text
// on the page rather than opening a new tab.
IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceBrowserTest,
                       CitationTextHighlight_DoesNotOpenNewTab) {
  GURL url("data:text/html,<html><body>some text to highlight</body></html>");
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));

  // Ensure the tab is open and on the expected URL.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  EXPECT_EQ(active_tab->GetContents()->GetVisibleURL(), url);

  // Create a task and associate the tab with the new task.
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTask task = contextual_tasks_service->CreateTask();
  contextual_tasks_service->AssociateTabWithTask(
      task.GetTaskId(),
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents()));

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());

  // Add a text fragment to the URL to mimic citation behavior.
  GURL citation_url = GURL(url.spec() + "#:~:text=highlight");

  // The text on the existing page should have highlights.
  companion::TextHighlighterManager* text_highlighter_manager =
      companion::TextHighlighterManager::GetForPage(
          active_tab->GetContents()->GetPrimaryPage());
  EXPECT_EQ(nullptr, text_highlighter_manager);

  // Fake a thread link click without necessarily having to open the side panel
  // and negotiate all those moving pieces.
  ui_service->OnThreadLinkClicked(citation_url, task.GetTaskId(), nullptr,
                                  browser()->GetWeakPtr(), url::Origin());

  // Wait for the TextHighlighterManager to be created.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    text_highlighter_manager = companion::TextHighlighterManager::GetForPage(
        active_tab->GetContents()->GetPrimaryPage());
    return text_highlighter_manager;
  }));

  // Ensure the tab count hasn't changed.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);

  // There should now be a highlighter for the page.
  EXPECT_EQ(
      1u, text_highlighter_manager->get_text_highlighters_for_testing().size());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceBrowserTest,
                       CitationTextHighlight_TextNotFoundOpensTab) {
  GURL url("data:text/html,<html><body>some text to highlight</body></html>");
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));

  // Ensure the tab is open and on the expected URL.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  EXPECT_EQ(active_tab->GetContents()->GetVisibleURL(), url);

  // Create a task and associate the tab with the new task.
  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTask task = contextual_tasks_service->CreateTask();
  contextual_tasks_service->AssociateTabWithTask(
      task.GetTaskId(),
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents()));

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());

  // Add a text fragment to the URL that does not contain text found in the
  // page.
  GURL citation_url = GURL(url.spec() + "#:~:text=google");

  // The text on the existing page should have highlights.
  companion::TextHighlighterManager* text_highlighter_manager =
      companion::TextHighlighterManager::GetForPage(
          active_tab->GetContents()->GetPrimaryPage());
  EXPECT_EQ(nullptr, text_highlighter_manager);

  // Fake a thread link click without necessarily having to open the side panel
  // and negotiate all those moving pieces.
  ui_service->OnThreadLinkClicked(citation_url, task.GetTaskId(), nullptr,
                                  browser()->GetWeakPtr(), url::Origin());

  // Wait for the new tab to be created.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return browser()->tab_strip_model()->count() == 3; }));

  // A text highlighter should not have been created for the existing page.
  text_highlighter_manager = companion::TextHighlighterManager::GetForPage(
      active_tab->GetContents()->GetPrimaryPage());
  EXPECT_EQ(nullptr, text_highlighter_manager);

  // Another tab should have been opened since the text wasn't found.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);
}

class TestLensMediaLinkHandler : public lens::LensMediaLinkHandler {
 public:
  explicit TestLensMediaLinkHandler(content::WebContents* web_contents)
      : LensMediaLinkHandler(web_contents) {}

  bool MaybeReplaceNavigation(const GURL& target) override { return true; }
};

class TestContextualTasksUiService : public ContextualTasksUiService {
 public:
  TestContextualTasksUiService(
      Profile* profile,
      contextual_tasks::ContextualTasksService* contextual_tasks_service,
      AimEligibilityService* aim_eligibility_service)
      : ContextualTasksUiService(
            profile,
            /*delegate=*/nullptr,
            contextual_tasks_service,
            /*identity_manager=*/nullptr,
            aim_eligibility_service,
            /*eligibility_manager=*/nullptr,
            std::unique_ptr<ContextualTasksCookieSynchronizer>()) {}
  ~TestContextualTasksUiService() override = default;

  MOCK_METHOD(std::unique_ptr<lens::LensMediaLinkHandler>,
              CreateMediaLinkHandler,
              (content::WebContents * web_contents),
              (override));
};

class ContextualTasksVideoCitationsBrowserTest : public InProcessBrowserTest {
 public:
  ContextualTasksVideoCitationsBrowserTest() {
    feature_list_.InitWithFeatures(
        {contextual_tasks::kContextualTasks,
         contextual_tasks::kContextualTasksVideoCitations},
        {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ContextualTasksVideoCitationsBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&ContextualTasksVideoCitationsBrowserTest::
                                         BuildTestContextualTasksUiService,
                                     base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> BuildTestContextualTasksUiService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<TestContextualTasksUiService>(
        profile, ContextualTasksServiceFactory::GetForProfile(profile),
        AimEligibilityServiceFactory::GetForProfile(profile));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksVideoCitationsBrowserTest,
                       VideoCitation_SeeksInsteadOfNavigating) {
  using testing::_;

  GURL url("data:text/html,<html><body></body></html>");
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));

  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  EXPECT_EQ(active_tab->GetContents()->GetVisibleURL(), url);

  ContextualTasksService* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->profile());
  ContextualTask task = contextual_tasks_service->CreateTask();
  contextual_tasks_service->AssociateTabWithTask(
      task.GetTaskId(),
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents()));

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());

  TestContextualTasksUiService* test_ui_service =
      static_cast<TestContextualTasksUiService*>(ui_service);

  EXPECT_CALL(*test_ui_service,
              CreateMediaLinkHandler(active_tab->GetContents()))
      .WillOnce([&](content::WebContents* wc) {
        return std::make_unique<TestLensMediaLinkHandler>(wc);
      });

  GURL youtube_url("https://www.youtube.com/watch?v=123");
  // Fake a thread link click.
  test_ui_service->OnThreadLinkClicked(youtube_url, task.GetTaskId(), nullptr,
                                       browser()->GetWeakPtr(), url::Origin());

  // Ensure the tab count hasn't changed.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
}

class ContextualTasksUiServiceZeroStateTestBase : public InProcessBrowserTest {
 public:
  ContextualTasksUiServiceZeroStateTestBase() = default;

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ContextualTasksUiServiceZeroStateTestBase::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&ContextualTasksUiServiceZeroStateTestBase::
                                BuildMockAimEligibilityService,
                            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto aim_eligibility_service = std::make_unique<MockAimEligibilityService>(
        *profile->GetPrefs(), /*template_url_service=*/nullptr,
        /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr);

    ON_CALL(*aim_eligibility_service, IsAimEligible())
        .WillByDefault(testing::Return(true));
    ON_CALL(*aim_eligibility_service, IsCobrowseEligible())
        .WillByDefault(testing::Return(true));
    ON_CALL(*aim_eligibility_service, HasAimUrlParams(testing::_))
        .WillByDefault(testing::Return(true));
    return aim_eligibility_service;
  }
  content::WebContents* OpenPanelAndGetContents(tabs::TabInterface* tab) {
    ContextualTasksUiService* ui_service =
        ContextualTasksUiServiceFactory::GetForBrowserContext(
            browser()->profile());
    GURL initial_url("https://example.com");
    ui_service->StartTaskUiInSidePanel(browser(), tab, initial_url, nullptr);

    auto* controller = ContextualTasksPanelController::From(browser());
    if (!base::test::RunUntil([&]() {
          return controller && controller->IsPanelOpenForContextualTask();
        })) {
      return nullptr;
    }
    content::WebContents* panel_contents = controller->GetActiveWebContents();
    if (!panel_contents) {
      return nullptr;
    }
    content::WaitForLoadStop(panel_contents);
    if (!GetWebUiInterface(panel_contents)) {
      return nullptr;
    }
    return panel_contents;
  }

  std::string GetTaskIdFromPanel(content::WebContents* panel_contents) {
    GURL panel_url = panel_contents->GetVisibleURL();
    std::string task_id_str;
    net::GetValueForKeyInQuery(panel_url, kTaskQueryParam, &task_id_str);
    return task_id_str;
  }

 protected:
  base::CallbackListSubscription create_services_subscription_;
};

class ContextualTasksUiServiceZeroStateEnabledTest
    : public ContextualTasksUiServiceZeroStateTestBase {
 public:
  ContextualTasksUiServiceZeroStateEnabledTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasks, {}},
         {omnibox::kWebUIOmniboxAskGAboutThisPage,
          {{"Omnibox_AskGCoBrowse", "true"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceZeroStateEnabledTest,
                       ZeroStateNavigation_ReloadsPanel) {
  GURL url("data:text/html,<html><body></body></html>");
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();

  content::WebContents* panel_contents = OpenPanelAndGetContents(active_tab);
  ASSERT_TRUE(panel_contents);

  std::string task_id_str = GetTaskIdFromPanel(panel_contents);
  ASSERT_FALSE(task_id_str.empty());

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  GURL zero_state_url = ui_service->GetDefaultAiPageUrl();

  content::TestNavigationObserver navigation_observer(panel_contents);
  ui_service->StartTaskUiInSidePanel(browser(), active_tab, zero_state_url,
                                     nullptr);
  navigation_observer.Wait();

  std::string new_task_id_str = GetTaskIdFromPanel(panel_contents);
  EXPECT_NE(task_id_str, new_task_id_str);
}

class ContextualTasksUiServiceZeroStateDisabledTest
    : public ContextualTasksUiServiceZeroStateTestBase {
 public:
  ContextualTasksUiServiceZeroStateDisabledTest() {
    feature_list_.InitWithFeatures({contextual_tasks::kContextualTasks},
                                   {omnibox::kWebUIOmniboxAskGAboutThisPage});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceZeroStateDisabledTest,
                       ZeroStateNavigation_DoesNotReloadPanel) {
  GURL url("data:text/html,<html><body></body></html>");
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();

  content::WebContents* panel_contents = OpenPanelAndGetContents(active_tab);
  ASSERT_TRUE(panel_contents);

  std::string task_id_str = GetTaskIdFromPanel(panel_contents);
  ASSERT_FALSE(task_id_str.empty());

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  GURL zero_state_url = ui_service->GetDefaultAiPageUrl();

  content::TestNavigationObserver navigation_observer(panel_contents);
  ui_service->StartTaskUiInSidePanel(browser(), active_tab, zero_state_url,
                                     nullptr);

  // We expect it to NOT reload because feature is disabled.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(navigation_observer.last_navigation_succeeded());

  std::string new_task_id_str = GetTaskIdFromPanel(panel_contents);
  EXPECT_EQ(task_id_str, new_task_id_str);
}

class ContextualTasksUiServiceTaskReuseTest
    : public ContextualTasksUiServiceZeroStateTestBase {
 public:
  ContextualTasksUiServiceTaskReuseTest() {
    feature_list_.InitWithFeatures({contextual_tasks::kContextualTasks}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceTaskReuseTest,
                       TaskReuse_ByMstk) {
  // 1. Setup Tab 1
  GURL url1("data:text/html,<html><body>Tab 1</body></html>");
  ASSERT_TRUE(AddTabAtIndex(0, url1, ui::PAGE_TRANSITION_TYPED));
  tabs::TabInterface* tab1 = browser()->tab_strip_model()->GetActiveTab();

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());

  // 2. Start task on Tab 1 with mstk
  GURL launch_url1("https://google.com/aim?mstk=abc");
  ui_service->StartTaskUiInSidePanel(
      browser(), tab1, launch_url1, /*session_handle=*/nullptr,
      /*associate_web_contents=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_mstk_for_task_association=*/true);

  auto* controller = ContextualTasksPanelController::From(browser());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return controller && controller->IsPanelOpenForContextualTask();
  }));
  content::WebContents* panel_contents1 = controller->GetActiveWebContents();
  ASSERT_TRUE(panel_contents1);
  content::WaitForLoadStop(panel_contents1);

  std::string task_id1 = GetTaskIdFromPanel(panel_contents1);
  ASSERT_FALSE(task_id1.empty());

  // 3. Setup Tab 2
  GURL url2("data:text/html,<html><body>Tab 2</body></html>");
  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_TYPED));
  tabs::TabInterface* tab2 = browser()->tab_strip_model()->GetActiveTab();
  EXPECT_NE(tab1, tab2);

  // 4. Start task on Tab 2 with same mstk
  GURL launch_url2("https://google.com/aim?mstk=abc");
  ui_service->StartTaskUiInSidePanel(
      browser(), tab2, launch_url2, /*session_handle=*/nullptr,
      /*associate_web_contents=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_mstk_for_task_association=*/true);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return controller && controller->IsPanelOpenForContextualTask();
  }));
  content::WebContents* panel_contents2 = controller->GetActiveWebContents();
  ASSERT_TRUE(panel_contents2);
  content::WaitForLoadStop(panel_contents2);

  std::string task_id2 = GetTaskIdFromPanel(panel_contents2);
  EXPECT_EQ(task_id1, task_id2);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUiServiceTaskReuseTest,
                       NoTaskReuse_WithoutFlag) {
  // 1. Setup Tab 1
  GURL url1("data:text/html,<html><body>Tab 1</body></html>");
  ASSERT_TRUE(AddTabAtIndex(0, url1, ui::PAGE_TRANSITION_TYPED));
  tabs::TabInterface* tab1 = browser()->tab_strip_model()->GetActiveTab();

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());

  // 2. Start task on Tab 1 with mstk, flag=true (so it registers in map)
  GURL launch_url1("https://google.com/aim?mstk=abc");
  ui_service->StartTaskUiInSidePanel(
      browser(), tab1, launch_url1, /*session_handle=*/nullptr,
      /*associate_web_contents=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_mstk_for_task_association=*/true);

  auto* controller = ContextualTasksPanelController::From(browser());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return controller && controller->IsPanelOpenForContextualTask();
  }));
  content::WebContents* panel_contents1 = controller->GetActiveWebContents();
  ASSERT_TRUE(panel_contents1);
  content::WaitForLoadStop(panel_contents1);

  std::string task_id1 = GetTaskIdFromPanel(panel_contents1);
  ASSERT_FALSE(task_id1.empty());

  // 3. Setup Tab 2
  GURL url2("data:text/html,<html><body>Tab 2</body></html>");
  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_TYPED));
  tabs::TabInterface* tab2 = browser()->tab_strip_model()->GetActiveTab();

  // 4. Start task on Tab 2 with same mstk, flag=false
  GURL launch_url2("https://google.com/aim?mstk=abc");
  ui_service->StartTaskUiInSidePanel(
      browser(), tab2, launch_url2, /*session_handle=*/nullptr,
      /*associate_web_contents=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_mstk_for_task_association=*/false);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return controller && controller->IsPanelOpenForContextualTask();
  }));
  content::WebContents* panel_contents2 = controller->GetActiveWebContents();
  ASSERT_TRUE(panel_contents2);
  content::WaitForLoadStop(panel_contents2);

  std::string task_id2 = GetTaskIdFromPanel(panel_contents2);
  EXPECT_NE(task_id1, task_id2);
}

}  // namespace contextual_tasks
