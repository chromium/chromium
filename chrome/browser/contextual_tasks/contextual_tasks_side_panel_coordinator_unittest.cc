// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
      : ContextualTasksUiService(nullptr, controller, nullptr) {}
  ~MockContextualTasksUiService() override = default;

  MOCK_METHOD(GURL,
              GetContextualTaskUrlForTask,
              (const base::Uuid& task_id),
              (override));
};

class MockSidePanelUI : public SidePanelUI {
 public:
  MOCK_METHOD(void,
              Show,
              (SidePanelEntryId entry_id,
               std::optional<SidePanelOpenTrigger> open_trigger,
               bool suppress_animations),
              (override));
  MOCK_METHOD(void,
              Show,
              (SidePanelEntryKey entry_key,
               std::optional<SidePanelOpenTrigger> open_trigger,
               bool suppress_animations),
              (override));
  MOCK_METHOD(void,
              ShowFrom,
              (SidePanelEntryKey entry_key,
               gfx::Rect starting_bounds_in_browser_coordinates),
              (override));
  MOCK_METHOD(void,
              Close,
              (SidePanelEntry::PanelType panel_type,
               SidePanelEntryHideReason hide_reason,
               bool suppress_animations),
              (override));
  MOCK_METHOD(void,
              Toggle,
              (SidePanelEntryKey key, SidePanelOpenTrigger open_trigger),
              (override));
  MOCK_METHOD(std::optional<SidePanelEntryId>,
              GetCurrentEntryId,
              (SidePanelEntry::PanelType panel_type),
              (const, override));
  MOCK_METHOD(int,
              GetCurrentEntryDefaultContentWidth,
              (SidePanelEntry::PanelType type),
              (const, override));
  MOCK_METHOD(bool,
              IsSidePanelShowing,
              (SidePanelEntry::PanelType type),
              (const, override));
  MOCK_METHOD(bool,
              IsSidePanelEntryShowing,
              (const SidePanelEntryKey& entry_key),
              (const, override));
  MOCK_METHOD(bool,
              IsSidePanelEntryShowing,
              (const SidePanelEntry::Key& entry_key, bool for_tab),
              (const, override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterSidePanelShown,
              (SidePanelEntry::PanelType type, ShownCallback callback),
              (override));
  MOCK_METHOD(content::WebContents*,
              GetWebContentsForTest,
              (SidePanelEntryId id),
              (override));
  MOCK_METHOD(void, DisableAnimationsForTesting, (), (override));
  MOCK_METHOD(void,
              SetNoDelaysForTesting,
              (bool no_delays_for_testing),
              (override));
};

class MockActiveTaskContextProvider : public ActiveTaskContextProvider {
 public:
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(void, OnSidePanelStateUpdated, (), (override));
  MOCK_METHOD(void, OnFullTabStateUpdated, (), (override));
  MOCK_METHOD(void, SetSessionHandleGetter, (SessionHandleGetter), (override));
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
    ON_CALL(*browser_window_, GetFeatures())
        .WillByDefault(ReturnRef(browser_window_features_));
    ON_CALL(Const(*browser_window_), GetFeatures())
        .WillByDefault(ReturnRef(browser_window_features_));

    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), profile_.get());
    ON_CALL(*browser_window_, GetTabStripModel())
        .WillByDefault(Return(tab_strip_model_.get()));

    ON_CALL(*browser_window_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));

    // Create SidePanelRegistry.
    side_panel_registry_ =
        std::make_unique<SidePanelRegistry>(browser_window_.get());

    coordinator_ = std::make_unique<ContextualTasksSidePanelCoordinator>(
        browser_window_.get(), &mock_side_panel_ui_,
        &mock_active_task_context_provider_);
  }

  void TearDown() override {
    coordinator_.reset();
    side_panel_registry_.reset();
    tab_strip_model_.reset();
    browser_window_.reset();
    mock_controller_ = nullptr;
    profile_.reset();
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
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  std::unique_ptr<TestingProfile> profile_;
  BrowserWindowFeatures browser_window_features_;
  std::unique_ptr<NiceMock<MockBrowserWindowInterface>> browser_window_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<SidePanelRegistry> side_panel_registry_;
  NiceMock<MockSidePanelUI> mock_side_panel_ui_;
  NiceMock<MockActiveTaskContextProvider> mock_active_task_context_provider_;
  raw_ptr<MockContextualTasksService> mock_controller_ = nullptr;

  std::unique_ptr<ContextualTasksSidePanelCoordinator> coordinator_;
};

TEST_F(ContextualTasksSidePanelCoordinatorTest,
       OpenSidePanelWillCreateNewTask) {
  // Create a new tab.
  tab_strip_model_->AppendWebContents(
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr),
      /*foreground=*/true);
  EXPECT_EQ(1, tab_strip_model_->count());

  ON_CALL(*browser_window_, GetActiveTabInterface())
      .WillByDefault(Return(tab_strip_model_->GetActiveTab()));

  ContextualTask expected_task(base::Uuid::GenerateRandomV4());
  ON_CALL(*mock_controller_, CreateTask()).WillByDefault(Return(expected_task));

  // Verify open the side panel with active tab not associated with a task will
  // create a new task.
  EXPECT_CALL(*mock_controller_, CreateTask()).Times(1);

  coordinator_->Show(false);
}

}  // namespace contextual_tasks
