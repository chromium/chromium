// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host_desktop.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace contextual_tasks {

namespace {

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
              (SidePanelEntryHideReason hide_reason, bool suppress_animations),
              (override));
  MOCK_METHOD(void,
              Toggle,
              (SidePanelEntryKey key, SidePanelOpenTrigger open_trigger),
              (override));
  MOCK_METHOD(std::optional<SidePanelEntryId>,
              GetCurrentEntryId,
              (),
              (const, override));
  MOCK_METHOD(int, GetCurrentEntryDefaultContentWidth, (), (const, override));
  MOCK_METHOD(bool, IsSidePanelShowing, (), (const, override));
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
              (ShownCallback callback),
              (override));
  MOCK_METHOD(void,
              OnActiveTabChanged,
              (content::WebContents * old_contents,
               content::WebContents* new_contents,
               bool tab_removed_for_deletion),
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

class MockContextualTasksPanelHostObserver
    : public ContextualTasksPanelHost::Observer {
 public:
  MOCK_METHOD(void,
              OnSurfaceStateChanged,
              (ContextualTasksPanelHost::SurfaceState state,
               ContextualTasksPanelHost::StateChangeReason reason),
              (override));
};

}  // namespace

class ContextualTasksPanelHostDesktopTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    browser_window_ = std::make_unique<NiceMock<MockBrowserWindowInterface>>();

    ON_CALL(*browser_window_, GetProfile())
        .WillByDefault(Return(profile_.get()));
    ON_CALL(*browser_window_, GetFeatures())
        .WillByDefault(ReturnRef(browser_window_features_));

    ON_CALL(*browser_window_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));

    // Create SidePanelRegistry.
    side_panel_registry_ =
        std::make_unique<SidePanelRegistry>(browser_window_.get());

    panel_host_ = std::make_unique<ContextualTasksPanelHostDesktop>(
        browser_window_.get(), &mock_side_panel_ui_);
  }

  void TearDown() override {
    panel_host_.reset();
    side_panel_registry_.reset();
    browser_window_.reset();
    profile_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  BrowserWindowFeatures browser_window_features_;
  std::unique_ptr<NiceMock<MockBrowserWindowInterface>> browser_window_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<SidePanelRegistry> side_panel_registry_;
  NiceMock<MockSidePanelUI> mock_side_panel_ui_;
  std::unique_ptr<ContextualTasksPanelHostDesktop> panel_host_;
};

TEST_F(ContextualTasksPanelHostDesktopTest, ShowCallsSidePanelUI) {
  NiceMock<MockContextualTasksPanelHostObserver> observer;
  panel_host_->AddObserver(&observer);

  EXPECT_CALL(mock_side_panel_ui_, IsSidePanelEntryShowing(_))
      .WillOnce(Return(false));
  EXPECT_CALL(
      mock_side_panel_ui_,
      Show(SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks), _, _))
      .Times(1);

  panel_host_->Show(ContextualTasksPanelHost::AnimationStyle::kStandard);
}

TEST_F(ContextualTasksPanelHostDesktopTest, ShowNoAnimation) {
  NiceMock<MockContextualTasksPanelHostObserver> observer;
  panel_host_->AddObserver(&observer);

  EXPECT_CALL(mock_side_panel_ui_, IsSidePanelEntryShowing(_))
      .WillOnce(Return(false));
  EXPECT_CALL(
      mock_side_panel_ui_,
      Show(SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks), _, true))
      .Times(1);

  panel_host_->Show(ContextualTasksPanelHost::AnimationStyle::kNoAnimation);
}

TEST_F(ContextualTasksPanelHostDesktopTest, ShowAlreadyOpenDoesNothing) {
  NiceMock<MockContextualTasksPanelHostObserver> observer;
  panel_host_->AddObserver(&observer);

  ON_CALL(mock_side_panel_ui_, IsSidePanelEntryShowing(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(
      mock_side_panel_ui_,
      Show(SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks), _, _))
      .Times(0);

  panel_host_->Show(ContextualTasksPanelHost::AnimationStyle::kStandard);
}

TEST_F(ContextualTasksPanelHostDesktopTest, CloseCallsSidePanelUI) {
  NiceMock<MockContextualTasksPanelHostObserver> observer;
  panel_host_->AddObserver(&observer);

  EXPECT_CALL(mock_side_panel_ui_,
              Close(SidePanelEntryHideReason::kSidePanelClosed, false))
      .Times(1);

  panel_host_->Close(ContextualTasksPanelHost::AnimationStyle::kStandard);
}

TEST_F(ContextualTasksPanelHostDesktopTest, CloseNoAnimation) {
  NiceMock<MockContextualTasksPanelHostObserver> observer;
  panel_host_->AddObserver(&observer);

  EXPECT_CALL(mock_side_panel_ui_,
              Close(SidePanelEntryHideReason::kSidePanelClosed, true))
      .Times(1);

  panel_host_->Close(ContextualTasksPanelHost::AnimationStyle::kNoAnimation);
}

TEST_F(ContextualTasksPanelHostDesktopTest, IsPanelOpenForContextualTask) {
  EXPECT_CALL(mock_side_panel_ui_, IsSidePanelEntryShowing(SidePanelEntry::Key(
                                       SidePanelEntry::Id::kContextualTasks)))
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_TRUE(panel_host_->IsPanelOpenForContextualTask());
  EXPECT_FALSE(panel_host_->IsPanelOpenForContextualTask());
}

TEST_F(ContextualTasksPanelHostDesktopTest,
       IsContextualTasksPanelSuppressedByDefaultWhenGlicIsShowing) {
  // By default, it should check for SidePanelEntry::Id::kGlic.
  EXPECT_CALL(
      mock_side_panel_ui_,
      IsSidePanelEntryShowing(SidePanelEntry::Key(SidePanelEntry::Id::kGlic)))
      .WillOnce(Return(true));

  EXPECT_TRUE(panel_host_->IsPanelSuppressed());
}

TEST_F(ContextualTasksPanelHostDesktopTest, NotifyObserversOnEntryShown) {
  NiceMock<MockContextualTasksPanelHostObserver> observer;
  panel_host_->AddObserver(&observer);

  EXPECT_CALL(observer,
              OnSurfaceStateChanged(
                  ContextualTasksPanelHost::SurfaceState::kVisible,
                  ContextualTasksPanelHost::StateChangeReason::kUserAction))
      .Times(1);

  panel_host_->OnEntryShown(nullptr);
}

TEST_F(ContextualTasksPanelHostDesktopTest, NotifyObserversOnEntryHidden) {
  NiceMock<MockContextualTasksPanelHostObserver> observer;
  panel_host_->AddObserver(&observer);

  EXPECT_CALL(observer,
              OnSurfaceStateChanged(
                  ContextualTasksPanelHost::SurfaceState::kClosed,
                  ContextualTasksPanelHost::StateChangeReason::kUserAction))
      .Times(1);

  panel_host_->OnEntryHidden(nullptr);
}

TEST_F(ContextualTasksPanelHostDesktopTest,
       CreateSidePanelViewInitializesWebView) {
  NiceMock<MockContextualTasksPanelHostObserver> observer;
  panel_host_->AddObserver(&observer);

  EXPECT_CALL(observer,
              OnSurfaceStateChanged(
                  ContextualTasksPanelHost::SurfaceState::kVisible,
                  ContextualTasksPanelHost::StateChangeReason::kSystemAction))
      .Times(1);

  std::unique_ptr<views::View> view =
      panel_host_->CreateSidePanelView(*side_panel_registry_);
  EXPECT_TRUE(view);
}

TEST_F(ContextualTasksPanelHostDesktopTest, WebContentsManagement) {
  std::unique_ptr<views::View> view =
      panel_host_->CreateSidePanelView(*side_panel_registry_);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);

  EXPECT_EQ(panel_host_->GetWebContents(), nullptr);
  EXPECT_TRUE(panel_host_->IsPanelInitialized());
  panel_host_->SetWebContents(web_contents.get());
  EXPECT_EQ(panel_host_->GetWebContents(), web_contents.get());
}

TEST_F(ContextualTasksPanelHostDesktopTest, IsPanelInitialized) {
  EXPECT_FALSE(panel_host_->IsPanelInitialized());

  std::unique_ptr<views::View> view =
      panel_host_->CreateSidePanelView(*side_panel_registry_);
  EXPECT_TRUE(panel_host_->IsPanelInitialized());
}

}  // namespace contextual_tasks
