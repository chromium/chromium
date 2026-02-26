// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/active_task_context_provider_impl.h"

#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_controller.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/range/range.h"

namespace contextual_tasks {

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

class MockObserver : public ActiveTaskContextProvider::Observer {
 public:
  MOCK_METHOD(void,
              OnContextTabsChanged,
              (const std::set<tabs::TabHandle>&),
              (override));
};

class ActiveTaskContextProviderImplTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    browser_window_ = std::make_unique<NiceMock<MockBrowserWindowInterface>>();

    ContextualTasksServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindOnce([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<NiceMock<MockContextualTasksService>>());
        }));
    contextual_tasks_service_ = static_cast<MockContextualTasksService*>(
        ContextualTasksServiceFactory::GetForProfile(profile_.get()));

    ON_CALL(*browser_window_, GetProfile())
        .WillByDefault(Return(profile_.get()));
    ON_CALL(*browser_window_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));

    tab_list_ = std::make_unique<NiceMock<MockTabListInterface>>();
    ON_CALL(*tab_list_, AddTabListInterfaceObserver(_))
        .WillByDefault([this](TabListInterfaceObserver* observer) {
          tab_list_observers_.AddObserver(observer);
        });
    ON_CALL(*tab_list_, RemoveTabListInterfaceObserver(_))
        .WillByDefault([this](TabListInterfaceObserver* observer) {
          tab_list_observers_.RemoveObserver(observer);
        });

    contextual_tasks_panel_controller_ =
        std::make_unique<NiceMock<MockContextualTasksPanelController>>();

    // Inject into host using production-compatible keys.
    tab_list_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
            unowned_user_data_host_, *tab_list_);

    provider_ = std::make_unique<ActiveTaskContextProviderImpl>(
        browser_window_.get(), contextual_tasks_service_);
    provider_->SetContextualTasksPanelController(
        contextual_tasks_panel_controller_.get());
    provider_->AddObserver(&observer_);
  }

  void TearDown() override {
    provider_->RemoveObserver(&observer_);
    provider_.reset();
    contextual_tasks_panel_controller_.reset();
    tab_list_registration_.reset();
    tab_list_.reset();
    contextual_tasks_service_ = nullptr;
    browser_window_.reset();

    // WebContents must be destroyed before Profile.
    web_contents_.clear();
    tabs_.clear();

    profile_.reset();
  }

  tabs::TabInterface* CreateMockTab() {
    auto tab = std::make_unique<tabs::MockTabInterface>();
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);
    content::WebContents* web_contents_ptr = web_contents.get();
    sessions::SessionTabHelper::CreateForWebContents(web_contents_ptr,
                                                     base::NullCallback());
    ContextualSearchWebContentsHelper::CreateForWebContents(web_contents_ptr);

    ON_CALL(*tab, GetContents()).WillByDefault(Return(web_contents_ptr));

    tabs::TabInterface* tab_ptr = tab.get();
    tabs_.push_back(std::move(tab));
    web_contents_.push_back(std::move(web_contents));
    return tab_ptr;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_;
  raw_ptr<MockContextualTasksService> contextual_tasks_service_;

  std::unique_ptr<MockTabListInterface> tab_list_;
  std::unique_ptr<ui::ScopedUnownedUserData<TabListInterface>>
      tab_list_registration_;

  std::unique_ptr<MockContextualTasksPanelController>
      contextual_tasks_panel_controller_;

  base::ObserverList<TabListInterfaceObserver> tab_list_observers_;
  std::unique_ptr<ActiveTaskContextProviderImpl> provider_;
  NiceMock<MockObserver> observer_;
  contextual_search::MockContextualSearchSessionHandle dummy_handle_;
  std::vector<std::unique_ptr<tabs::TabInterface>> tabs_;
  std::vector<std::unique_ptr<content::WebContents>> web_contents_;
};

TEST_F(ActiveTaskContextProviderImplTest, RefreshContextNoTaskId) {
  EXPECT_CALL(*contextual_tasks_panel_controller_,
              GetSessionHandleForActiveTabOrSidePanel())
      .WillOnce(Return(std::make_pair(std::nullopt, nullptr)));
  EXPECT_CALL(observer_, OnContextTabsChanged(std::set<tabs::TabHandle>()))
      .Times(1);

  provider_->RefreshContext();
}

TEST_F(ActiveTaskContextProviderImplTest, RefreshContextWithTabs) {
  tabs::TabInterface* tab1 = CreateMockTab();
  SessionID id1 = sessions::SessionTabHelper::IdForTab(tab1->GetContents());

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  UrlResource resource(GURL("https://example.com"), ResourceType::kWebpage);
  resource.tab_id = id1;
  task.AddUrlResource(resource);

  EXPECT_CALL(*contextual_tasks_panel_controller_,
              GetSessionHandleForActiveTabOrSidePanel())
      .WillOnce(Return(std::make_pair(task_id, &dummy_handle_)));

  EXPECT_CALL(*contextual_tasks_service_, GetContextForTask(task_id, _, _, _))
      .WillOnce([&task](const base::Uuid&,
                        const std::set<ContextualTaskContextSource>&,
                        std::unique_ptr<ContextDecorationParams>,
                        base::OnceCallback<void(
                            std::unique_ptr<ContextualTaskContext>)> callback) {
        std::move(callback).Run(std::make_unique<ContextualTaskContext>(task));
      });

  EXPECT_CALL(*tab_list_, GetTabCount()).WillRepeatedly(Return(1));
  EXPECT_CALL(*tab_list_, GetTab(0)).WillRepeatedly(Return(tab1));

  std::set<tabs::TabHandle> expected_tabs = {tab1->GetHandle()};
  EXPECT_CALL(observer_, OnContextTabsChanged(expected_tabs)).Times(1);

  provider_->RefreshContext();
  task_environment_.RunUntilIdle();
}

TEST_F(ActiveTaskContextProviderImplTest, AutoSuggestedTab) {
  tabs::TabInterface* tab1 = CreateMockTab();

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);

  EXPECT_CALL(*contextual_tasks_panel_controller_,
              GetSessionHandleForActiveTabOrSidePanel())
      .WillOnce(Return(std::make_pair(task_id, &dummy_handle_)));

  EXPECT_CALL(*contextual_tasks_service_, GetContextForTask(task_id, _, _, _))
      .WillOnce([&task](const base::Uuid&,
                        const std::set<ContextualTaskContextSource>&,
                        std::unique_ptr<ContextDecorationParams>,
                        base::OnceCallback<void(
                            std::unique_ptr<ContextualTaskContext>)> callback) {
        std::move(callback).Run(std::make_unique<ContextualTaskContext>(task));
      });

  // Mock controller behavior.
  EXPECT_CALL(*contextual_tasks_panel_controller_,
              IsPanelOpenForContextualTask())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*contextual_tasks_panel_controller_, GetAutoSuggestedTabHandle())
      .WillRepeatedly(Return(tab1->GetHandle()));

  // Expected tabs should include tab1.
  std::set<tabs::TabHandle> expected_tabs = {tab1->GetHandle()};
  EXPECT_CALL(observer_, OnContextTabsChanged(expected_tabs)).Times(1);

  provider_->RefreshContext();
  task_environment_.RunUntilIdle();
}

TEST_F(ActiveTaskContextProviderImplTest, RefreshContextStaleCallback) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);

  base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
      captured_callback;

  EXPECT_CALL(*contextual_tasks_panel_controller_,
              GetSessionHandleForActiveTabOrSidePanel())
      .WillRepeatedly(Return(std::make_pair(task_id, &dummy_handle_)));

  EXPECT_CALL(*contextual_tasks_service_, GetContextForTask(task_id, _, _, _))
      .WillOnce(
          [&captured_callback](
              const base::Uuid&, const std::set<ContextualTaskContextSource>&,
              std::unique_ptr<ContextDecorationParams>,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { captured_callback = std::move(callback); })
      .WillOnce(
          [](const base::Uuid&, const std::set<ContextualTaskContextSource>&,
             std::unique_ptr<ContextDecorationParams>,
             base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                 callback) {
            std::move(callback).Run(std::make_unique<ContextualTaskContext>(
                ContextualTask(base::Uuid::GenerateRandomV4())));
          });

  // First call to RefreshContext.
  provider_->RefreshContext();
  EXPECT_TRUE(captured_callback);

  // Second call to RefreshContext should invalidate the first callback.
  // This second call will trigger OnContextTabsChanged immediately due to our
  // mock.
  EXPECT_CALL(observer_, OnContextTabsChanged(_)).Times(1);
  provider_->RefreshContext();

  // Run the first (stale) callback. It should NOT trigger OnContextTabsChanged.
  EXPECT_CALL(observer_, OnContextTabsChanged(_)).Times(0);
  std::move(captured_callback)
      .Run(std::make_unique<ContextualTaskContext>(task));

  task_environment_.RunUntilIdle();
}

TEST_F(ActiveTaskContextProviderImplTest, ServiceObserversTriggerRefresh) {
  // Set expectations for RefreshContext which will be called by each observer.
  EXPECT_CALL(*contextual_tasks_panel_controller_,
              GetSessionHandleForActiveTabOrSidePanel())
      .WillRepeatedly(Return(std::make_pair(std::nullopt, nullptr)));
  EXPECT_CALL(observer_, OnContextTabsChanged(_)).Times(5);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  provider_->OnTaskAdded(task, ContextualTasksService::TriggerSource::kUnknown);
  provider_->OnTaskUpdated(task,
                           ContextualTasksService::TriggerSource::kUnknown);
  provider_->OnTaskRemoved(task.GetTaskId(),
                           ContextualTasksService::TriggerSource::kUnknown);
  provider_->OnTaskAssociatedToTab(task.GetTaskId(), SessionID::NewUnique());
  provider_->OnTaskDisassociatedFromTab(task.GetTaskId(),
                                        SessionID::NewUnique());
}

TEST_F(ActiveTaskContextProviderImplTest, ActiveTabChanged) {
  tabs::TabInterface* tab1 = CreateMockTab();

  // When active tab changes, RefreshContext is called.
  EXPECT_CALL(*contextual_tasks_panel_controller_,
              GetSessionHandleForActiveTabOrSidePanel())
      .WillOnce(Return(std::make_pair(std::nullopt, nullptr)));
  EXPECT_CALL(observer_, OnContextTabsChanged(_)).Times(1);

  // Simulate active tab change.
  for (auto& observer : tab_list_observers_) {
    observer.OnActiveTabChanged(*tab_list_, tab1);
  }
}

TEST_F(ActiveTaskContextProviderImplTest, PrimaryPageChanged) {
  tabs::TabInterface* tab = CreateMockTab();

  // When active tab changes, RefreshContext is called.
  EXPECT_CALL(*contextual_tasks_panel_controller_,
              GetSessionHandleForActiveTabOrSidePanel())
      .WillRepeatedly(Return(std::make_pair(std::nullopt, nullptr)));
  EXPECT_CALL(observer_, OnContextTabsChanged(_)).Times(1);

  for (auto& observer : tab_list_observers_) {
    observer.OnActiveTabChanged(*tab_list_, tab);
  }

  // Simulate primary page change on the active tab.
  EXPECT_CALL(observer_, OnContextTabsChanged(_)).Times(1);
  provider_->PrimaryPageChanged(tab->GetContents()->GetPrimaryPage());
}

}  // namespace contextual_tasks
