// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/startup/automation_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ChromeDevToolsManagerDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitAndDisableFeature(infobars::kCentralizedInfoBarFramework);
    profile_ = std::make_unique<TestingProfile>();
    tab_strip_ =
        std::make_unique<TabStripModel>(&tab_strip_delegate_, profile_.get());
    tab_strip_delegate_.SetBrowserWindowInterface(&window_);
    ON_CALL(window_, GetTabStripModel())
        .WillByDefault(testing::Return(tab_strip_.get()));
    ON_CALL(std::as_const(window_), GetTabStripModel())
        .WillByDefault(testing::Return(tab_strip_.get()));
    ON_CALL(window_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    ON_CALL(std::as_const(window_), GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    collection_observer()->OnBrowserCreated(&window_);
    ASSERT_FALSE(ChromeDevToolsManagerDelegate::GetInstance());
    delegate_ = std::make_unique<ChromeDevToolsManagerDelegate>();
  }

  void TearDown() override {
    delegate_.reset();
    tab_strip_->CloseAllTabs();
    collection_observer()->OnBrowserClosed(&window_);
    tab_strip_delegate_.SetBrowserWindowInterface(nullptr);
    tab_strip_.reset();
    profile_.reset();
  }

  infobars::ContentInfoBarManager* AddTab(
      bool with_automation_infobar = false) {
    auto contents = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    infobars::ContentInfoBarManager::CreateForWebContents(contents.get());
    auto* manager =
        infobars::ContentInfoBarManager::FromWebContents(contents.get());
    if (with_automation_infobar) {
      // The automation and remote debugging infobars have the same message,
      // so the manager rejects the latter as a duplicate.
      EXPECT_TRUE(AutomationInfoBarDelegate::Create(manager));
    }
    tab_strip_->AppendWebContents(std::move(contents), /*foreground=*/true);
    return manager;
  }

  void SetConnections(size_t count) {
    static_cast<content::DevToolsManagerDelegate*>(delegate_.get())
        ->SetActiveWebSocketConnections(count);
  }

  void ExpectReconnect(infobars::ContentInfoBarManager* manager) {
    manager->RemoveAllInfoBars(/*animate=*/false);
    SetConnections(1);
    ASSERT_EQ(1u, manager->infobars().size());
    EXPECT_EQ(
        infobars::InfoBarDelegate::DEV_TOOLS_REMOTE_DEBUGGING_INFOBAR_DELEGATE,
        manager->infobars()[0]->delegate()->GetIdentifier());
    SetConnections(0);
    EXPECT_TRUE(manager->infobars().empty());
  }

  std::unique_ptr<ChromeDevToolsManagerDelegate> delegate_;

 private:
  BrowserCollectionObserver* collection_observer() {
    return GlobalBrowserCollection::GetInstance()->GetPlatformDelegate();
  }

  content::BrowserTaskEnvironment task_environment_;
  ChromeLayoutProvider layout_provider_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_tab_features_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  TestTabStripModelDelegate tab_strip_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_;
  testing::NiceMock<MockBrowserWindowInterface> window_;
};

TEST_F(ChromeDevToolsManagerDelegateTest, DisconnectAfterInfoBarCreationFails) {
  auto* manager = AddTab(/*with_automation_infobar=*/true);
  SetConnections(1);
  // Let the global infobar delete itself after AddInfoBar() rejects it.
  base::RunLoop().RunUntilIdle();

  SetConnections(0);
  ASSERT_EQ(1u, manager->infobars().size());
  EXPECT_EQ(infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE,
            manager->infobars()[0]->delegate()->GetIdentifier());
  ExpectReconnect(manager);
}

TEST_F(ChromeDevToolsManagerDelegateTest, DisconnectAfterInfoBarFailsInNewTab) {
  auto* first_manager = AddTab();
  SetConnections(1);
  ASSERT_EQ(1u, first_manager->infobars().size());

  auto* second_manager = AddTab(/*with_automation_infobar=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(first_manager->infobars().empty());

  SetConnections(0);
  ASSERT_EQ(1u, second_manager->infobars().size());
  ExpectReconnect(second_manager);
  EXPECT_TRUE(first_manager->infobars().empty());
}

TEST_F(ChromeDevToolsManagerDelegateTest,
       DestroyDelegateAfterInfoBarCreationFails) {
  auto* manager = AddTab(/*with_automation_infobar=*/true);
  SetConnections(1);
  base::RunLoop().RunUntilIdle();

  delegate_.reset();
  ASSERT_EQ(1u, manager->infobars().size());
  EXPECT_EQ(infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE,
            manager->infobars()[0]->delegate()->GetIdentifier());
}

}  // namespace
