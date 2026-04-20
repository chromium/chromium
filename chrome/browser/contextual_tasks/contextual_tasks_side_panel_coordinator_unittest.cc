// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_host.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#endif

using testing::_;
using testing::Const;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace contextual_tasks {

namespace {

class MockContextualTasksUiService : public ContextualTasksUiService {
 public:
  explicit MockContextualTasksUiService(ContextualTasksService* controller)
      : ContextualTasksUiService(nullptr,
                                 nullptr,
                                 controller,
                                 nullptr,
                                 nullptr,
                                 /*cookie_synchronizer=*/nullptr) {}
  ~MockContextualTasksUiService() override = default;

  MOCK_METHOD(GURL,
              GetContextualTaskUrlForTask,
              (const base::Uuid& task_id),
              (override));
  MOCK_METHOD(void,
              SetInitialEntryPointForTask,
              (const base::Uuid& task_id,
               omnibox::ChromeAimEntryPoint entry_point),
              (override));
  MOCK_METHOD(GURL,
              GetDefaultAiPageUrlForTask,
              (const base::Uuid& task_id),
              (override));
};

class MockActiveTaskContextProvider : public ActiveTaskContextProvider {
 public:
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RefreshContext, (), (override));
  MOCK_METHOD(void,
              SetContextualTasksPanelController,
              (ContextualTasksPanelController*),
              (override));
};

}  // namespace

class ContextualTasksSidePanelCoordinatorTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    ContextualTasksServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating(&ContextualTasksSidePanelCoordinatorTest::
                                CreateMockContextController,
                            base::Unretained(this)));

    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          auto* controller = ContextualTasksServiceFactory::GetForProfile(
              Profile::FromBrowserContext(context));
          return std::make_unique<NiceMock<MockContextualTasksUiService>>(
              controller);
        }));

    browser_window_ = std::make_unique<NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*browser_window_, GetProfile())
        .WillByDefault(Return(profile_.get()));

#if !BUILDFLAG(IS_ANDROID)
    ON_CALL(*browser_window_, GetFeatures())
        .WillByDefault(ReturnRef(browser_window_features_));
    ON_CALL(Const(*browser_window_), GetFeatures())
        .WillByDefault(ReturnRef(browser_window_features_));

    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), profile_.get());
    ON_CALL(*browser_window_, GetTabStripModel())
        .WillByDefault(Return(tab_strip_model_.get()));
    ON_CALL(Const(*browser_window_), GetTabStripModel())
        .WillByDefault(Return(tab_strip_model_.get()));
#endif

    ON_CALL(*browser_window_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));

    tab_list_ = std::make_unique<NiceMock<MockTabListInterface>>();
    tab_list_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
            unowned_user_data_host_, *tab_list_);

    auto mock_panel_host =
        std::make_unique<NiceMock<MockContextualTasksPanelHost>>();
    ON_CALL(*mock_panel_host, IsPanelInitialized())
        .WillByDefault(testing::Return(true));
    mock_panel_host_ = mock_panel_host.get();

    coordinator_ = std::make_unique<ContextualTasksSidePanelCoordinator>(
        browser_window_.get(), std::move(mock_panel_host),
        &mock_active_task_context_provider_, nullptr);

    // Create a mock tab.
    tabs::TabInterface* tab_ptr = CreateMockTab();

    ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(tab_ptr));
    ON_CALL(*tab_list_, GetAllTabs()).WillByDefault([this]() {
      std::vector<tabs::TabInterface*> result;
      for (const auto& tab : tabs_) {
        result.push_back(tab.get());
      }
      return result;
    });

    ContextualTask expected_task(base::Uuid::GenerateRandomV4());
    ON_CALL(*mock_controller_, CreateTask())
        .WillByDefault(Return(expected_task));

#if !BUILDFLAG(IS_ANDROID)
    HatsServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildMockHatsService));
#endif
  }

  void TearDown() override {
    mock_controller_ = nullptr;
    mock_panel_host_ = nullptr;

    coordinator_.reset();
    tab_list_registration_.reset();
    tab_list_.reset();
    browser_window_.reset();

    // WebContents must be destroyed before Profile.
    web_contents_list_.clear();
    tabs_.clear();

    profile_.reset();
  }

  tabs::TabInterface* CreateMockTab() {
    auto tab = std::make_unique<tabs::MockTabInterface>();
    content::WebContents* web_contents_ptr =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr)
            .release();
    sessions::SessionTabHelper::CreateForWebContents(web_contents_ptr,
                                                     base::NullCallback());

    ON_CALL(*tab, GetContents()).WillByDefault(Return(web_contents_ptr));

    tabs::TabInterface* tab_ptr = tab.get();
    tabs_.push_back(std::move(tab));
    web_contents_list_.push_back(base::WrapUnique(web_contents_ptr));
    return tab_ptr;
  }

  void TriggerOnEligibilityChange(bool is_eligible) {
    coordinator_->OnEligibilityChange(is_eligible);
  }

  void CreateWebContentsForTesting() {
    coordinator_->CreateCachedWebContentsForTesting(
        base::Uuid::GenerateRandomV4(), true);
  }

  void CreateCachedWebContentsForTesting(base::Uuid task_id, bool is_open) {
    coordinator_->CreateCachedWebContentsForTesting(task_id, is_open);
  }

  void CleanUpUnusedWebContents() { coordinator_->CleanUpUnusedWebContents(); }

  bool UpdateWebContentsForActiveTab() {
    return coordinator_->UpdateWebContentsForActiveTab();
  }

  void ClearCacheForTesting() {
    coordinator_->task_id_to_web_contents_cache_.clear();
  }

  size_t GetNumberOfActiveTasksForTesting(base::Uuid task_id) {
    return coordinator_->task_id_to_web_contents_cache_.count(task_id);
  }

  content::WebContents* GetWebContentsForTaskForTesting(base::Uuid task_id) {
    auto it = coordinator_->task_id_to_web_contents_cache_.find(task_id);
    return it != coordinator_->task_id_to_web_contents_cache_.end()
               ? it->second->web_contents.get()
               : nullptr;
  }

  std::unique_ptr<KeyedService> CreateMockContextController(
      content::BrowserContext* context) {
    auto mock = std::make_unique<NiceMock<MockContextualTasksService>>();
    mock_controller_ = mock.get();
    return mock;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
#if !BUILDFLAG(IS_ANDROID)
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  BrowserWindowFeatures browser_window_features_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
#endif
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<NiceMock<MockBrowserWindowInterface>> browser_window_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<MockTabListInterface> tab_list_;
  std::unique_ptr<ui::ScopedUnownedUserData<TabListInterface>>
      tab_list_registration_;
  NiceMock<MockActiveTaskContextProvider> mock_active_task_context_provider_;
  raw_ptr<MockContextualTasksPanelHost> mock_panel_host_ = nullptr;
  raw_ptr<MockContextualTasksService> mock_controller_ = nullptr;
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<ContextualTasksSidePanelCoordinator> coordinator_;

  std::vector<std::unique_ptr<tabs::TabInterface>> tabs_;
  std::vector<std::unique_ptr<content::WebContents>> web_contents_list_;
};

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OpenSidePanelWillCreateNewTask) {
  // Verify open the side panel with active tab not associated with a task will
  // create a new task.
  EXPECT_CALL(*mock_controller_, CreateTask()).Times(1);
  coordinator_->Show(false,
                     omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, ShowSidePanelSetsEntryPoint) {
  ContextualTask task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillByDefault(Return(task));

  auto* mock_ui_service = static_cast<MockContextualTasksUiService*>(
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile_.get()));
  EXPECT_CALL(
      *mock_ui_service,
      SetInitialEntryPointForTask(
          _,
          omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON))
      .Times(1);
  coordinator_->Show(
      false,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest, CloseSidePanelWhenNotEligible) {
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));

  CreateWebContentsForTesting();
  EXPECT_EQ(1u, coordinator_->GetNumberOfActiveTasks());

  // Verify that the side panel is closed when not eligible and cache is empty.
  EXPECT_CALL(*mock_panel_host_,
              Close(ContextualTasksPanelHost::AnimationStyle::kStandard));
  TriggerOnEligibilityChange(false);
  EXPECT_EQ(0u, coordinator_->GetNumberOfActiveTasks());
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContextualTasksSidePanelCoordinatorTest,
       ShowSidePanelLaunchesSurveyArm1) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kContextualTasks,
      {{"ContextualTasksExpandButtonOptions", "side-panel-expand-button"}});
  base::test::ScopedFeatureList survey_feature_list;
  survey_feature_list.InitAndEnableFeature(
      features::kHappinessTrackingSurveysForDesktopNextPanel);
  profile_->GetPrefs()->SetInteger(prefs::kContextualTasksNextPanelOpenCount,
                                   1);

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetForProfile(profile_.get(), true));

  EXPECT_CALL(*mock_hats_service,
              LaunchDelayedSurvey(
                  kHatsSurveyTriggerNextPanel, 90000, testing::_,
                  testing::Contains(testing::Pair("Experiment ID", "Arm 1"))))
      .Times(1);

  coordinator_->Show(
      false,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
}

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       ShowSidePanelLaunchesSurveyArm5) {
  feature_list_.InitAndEnableFeatureWithParameters(
      kContextualTasks,
      {{"ContextualTasksExpandButtonOptions", "toolbar-close-button"}});
  base::test::ScopedFeatureList survey_feature_list;
  survey_feature_list.InitAndEnableFeature(
      features::kHappinessTrackingSurveysForDesktopNextPanel);
  profile_->GetPrefs()->SetInteger(prefs::kContextualTasksNextPanelOpenCount,
                                   1);

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetForProfile(profile_.get(), true));

  EXPECT_CALL(*mock_hats_service,
              LaunchDelayedSurvey(
                  kHatsSurveyTriggerNextPanel, 90000, testing::_,
                  testing::Contains(testing::Pair("Experiment ID", "Arm 5"))))
      .Times(1);

  coordinator_->Show(
      false,
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
}
#endif

TEST_F(ContextualTasksSidePanelCoordinatorTest, CleanUpUnusedWebContents) {
  // Clear any tasks from SetUp.
  ClearCacheForTesting();

  // Ensure mock_controller handles any task_id or tab_id by default.
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(_))
      .WillRepeatedly(Return(std::vector<SessionID>{}));
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(_))
      .WillRepeatedly(Return(std::nullopt));

  // task_id1: Not associated with any tab. Should be cleaned up immediately.
  base::Uuid task_id1 = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(task_id1, false);

  // task_id2: Associated with the active tab's web contents.
  base::Uuid task_id2 = base::Uuid::GenerateRandomV4();
  CreateCachedWebContentsForTesting(task_id2, true);

  // Ensure the coordinator knows task_id2 is associated with the active tab.
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  EXPECT_CALL(*mock_controller_, GetContextualTaskForTab(active_tab_id))
      .WillRepeatedly(Return(ContextualTask(task_id2)));
  EXPECT_CALL(*mock_controller_, GetTabsAssociatedWithTask(task_id2))
      .WillRepeatedly(Return(std::vector<SessionID>{active_tab_id}));

  // Ensure the coordinator knows task_id2 is the active one.
  UpdateWebContentsForActiveTab();
  ASSERT_EQ(coordinator_->GetActiveWebContents(),
            GetWebContentsForTaskForTesting(task_id2));

  EXPECT_EQ(2u, coordinator_->GetNumberOfActiveTasks());

  // 1. Clean up task_id1 immediately (no associated tabs).
  CleanUpUnusedWebContents();
  EXPECT_EQ(1u, coordinator_->GetNumberOfActiveTasks());
  EXPECT_EQ(0u, GetNumberOfActiveTasksForTesting(task_id1));

  // 2. task_id2 should STILL be there because it's active.
  EXPECT_EQ(1u, GetNumberOfActiveTasksForTesting(task_id2));
}

}  // namespace contextual_tasks
