// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

using testing::_;
using testing::NiceMock;
using testing::Return;

class ContextualTasksPageHandlerBrowserTest : public ::InProcessBrowserTest {
 public:
  ContextualTasksPageHandlerBrowserTest() {
    feature_list_.InitAndEnableFeature(kContextualTasksContextLibrary);
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ContextualTasksPageHandlerBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    ContextualTasksServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<NiceMock<MockContextualTasksService>>());
        }));

    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          Profile* profile = Profile::FromBrowserContext(context);
          return std::unique_ptr<KeyedService>(
              std::make_unique<NiceMock<MockContextualTasksUiService>>(
                  profile,
                  ContextualTasksServiceFactory::GetForProfile(profile)));
        }));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    profile_ = browser()->profile();

    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    web_ui_.set_web_contents(web_contents_);

    contextual_tasks_ui_ = std::make_unique<ContextualTasksUI>(&web_ui_);

    mock_contextual_tasks_service_ = static_cast<MockContextualTasksService*>(
        ContextualTasksServiceFactory::GetForProfile(profile_));
    mock_contextual_tasks_ui_service_ =
        static_cast<MockContextualTasksUiService*>(
            ContextualTasksUiServiceFactory::GetForBrowserContext(
                profile_.get()));

    page_handler_ = std::make_unique<ContextualTasksPageHandler>(
        mojo::PendingReceiver<mojom::PageHandler>(), contextual_tasks_ui_.get(),
        mock_contextual_tasks_ui_service_, mock_contextual_tasks_service_,
        nullptr);
    page_handler_->set_skip_feedback_ui_for_testing(true);
  }

  void TearDownOnMainThread() override {
    page_handler_.reset();
    contextual_tasks_ui_.reset();
    profile_ = nullptr;
    web_contents_ = nullptr;
    mock_contextual_tasks_service_ = nullptr;
    mock_contextual_tasks_ui_service_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  std::unique_ptr<ContextualTasksUI> contextual_tasks_ui_;
  std::unique_ptr<ContextualTasksPageHandler> page_handler_;
  raw_ptr<MockContextualTasksService> mock_contextual_tasks_service_;
  raw_ptr<MockContextualTasksUiService> mock_contextual_tasks_ui_service_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksPageHandlerBrowserTest, OpenFeedbackUi) {
  page_handler_->set_skip_feedback_ui_for_testing(false);

  EXPECT_CALL(*mock_contextual_tasks_ui_service_, OpenFeedbackUi(browser(), _))
      .Times(1);

  page_handler_->OpenFeedbackUi();
}

}  // namespace contextual_tasks
