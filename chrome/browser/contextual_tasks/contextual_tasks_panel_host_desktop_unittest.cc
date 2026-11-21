// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host_desktop.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/side_panel/mock_side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if !BUILDFLAG(IS_MAC)
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/widget/root_view.h"
#endif

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace contextual_tasks {

namespace {

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

class ContextualTasksPanelHostDesktopTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                            &test_url_loader_factory_));
    profile_ = builder.Build();
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
        browser_window_.get());
  }

  void TearDown() override {
    panel_host_.reset();
    side_panel_registry_.reset();
    browser_window_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  BrowserWindowFeatures browser_window_features_;
  std::unique_ptr<NiceMock<MockBrowserWindowInterface>> browser_window_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<SidePanelRegistry> side_panel_registry_;
  NiceMock<MockSidePanelUI> mock_side_panel_ui_{unowned_user_data_host_};
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

TEST_F(ContextualTasksPanelHostDesktopTest,
       GetToolbarWebContentsRearchitectureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kContextualTasksSidePanelRearchitecture);

  std::unique_ptr<views::View> view =
      panel_host_->CreateSidePanelView(*side_panel_registry_);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);
  panel_host_->SetWebContents(web_contents.get());
  EXPECT_EQ(panel_host_->GetToolbarWebContents(), web_contents.get());
}

TEST_F(ContextualTasksPanelHostDesktopTest,
       GetToolbarWebContentsRearchitectureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kContextualTasksSidePanelRearchitecture);

  std::unique_ptr<views::View> view =
      panel_host_->CreateSidePanelView(*side_panel_registry_);
  content::WebContents* toolbar_contents = panel_host_->GetToolbarWebContents();
  ASSERT_TRUE(toolbar_contents);
  EXPECT_EQ(webui::GetBrowserWindowInterface(toolbar_contents),
            browser_window_.get());

  // Simulate hiding the side panel on tab switch (which destroys the view) and
  // reopening it when switching back. The toolbar WebContents should persist
  // without being recreated or losing its BrowserWindowInterface association.
  view.reset();
  EXPECT_EQ(panel_host_->GetToolbarWebContents(), toolbar_contents);
  EXPECT_EQ(webui::GetBrowserWindowInterface(toolbar_contents),
            browser_window_.get());

  std::unique_ptr<views::View> recreated_view =
      panel_host_->CreateSidePanelView(*side_panel_registry_);
  EXPECT_EQ(panel_host_->GetToolbarWebContents(), toolbar_contents);
  EXPECT_EQ(webui::GetBrowserWindowInterface(toolbar_contents),
            browser_window_.get());
}

TEST_F(ContextualTasksPanelHostDesktopTest,
       AnnounceAlertDeliversImmediatelyWhenAttachedToWidget) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kContextualTasksSidePanelRearchitecture);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  std::unique_ptr<views::View> view =
      panel_host_->CreateSidePanelView(*side_panel_registry_);
  ContextualTasksWebView* web_view =
      views::AsViewClass<ContextualTasksWebView>(view.get());
  ASSERT_NE(web_view, nullptr);
  widget->SetContentsView(std::move(view));

#if !BUILDFLAG(IS_MAC)
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
#endif
  panel_host_->AnnounceAlert(u"Microphone in use");
  EXPECT_EQ(web_view->pending_alert_for_testing(), std::nullopt);

#if !BUILDFLAG(IS_MAC)
  if (!views::ViewAccessibility::IsViewsAccessibilityTreeEnabled()) {
    EXPECT_EQ(counter.GetCount(ax::mojom::Event::kAlert), 1);
    views::View* announce_view =
        static_cast<views::internal::RootView*>(widget->GetRootView())
            ->GetAnnounceViewForTesting();
    ASSERT_NE(announce_view, nullptr);
    EXPECT_EQ(announce_view->GetViewAccessibility().GetCachedName(),
              u"Microphone in use");
  }
#endif

  widget.reset();
}

TEST_F(ContextualTasksPanelHostDesktopTest,
       AnnounceAlertQueuesUntilAddedToWidget) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kContextualTasksSidePanelRearchitecture);

  std::unique_ptr<views::View> view =
      panel_host_->CreateSidePanelView(*side_panel_registry_);
  ContextualTasksWebView* web_view =
      views::AsViewClass<ContextualTasksWebView>(view.get());
  ASSERT_NE(web_view, nullptr);
  ASSERT_EQ(web_view->GetWidget(), nullptr);

#if !BUILDFLAG(IS_MAC)
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
#endif
  panel_host_->AnnounceAlert(u"Queued microphone alert");
  EXPECT_EQ(web_view->pending_alert_for_testing(),
            std::optional<std::u16string>(u"Queued microphone alert"));
#if !BUILDFLAG(IS_MAC)
  if (!views::ViewAccessibility::IsViewsAccessibilityTreeEnabled()) {
    EXPECT_EQ(counter.GetCount(ax::mojom::Event::kAlert), 0);
  }
#endif

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  views::View* container =
      widget->SetContentsView(std::make_unique<views::View>());
  container->AddChildView(std::move(view));

  EXPECT_EQ(web_view->pending_alert_for_testing(), std::nullopt);
#if !BUILDFLAG(IS_MAC)
  if (!views::ViewAccessibility::IsViewsAccessibilityTreeEnabled()) {
    EXPECT_EQ(counter.GetCount(ax::mojom::Event::kAlert), 1);
    views::View* announce_view =
        static_cast<views::internal::RootView*>(widget->GetRootView())
            ->GetAnnounceViewForTesting();
    ASSERT_NE(announce_view, nullptr);
    EXPECT_EQ(announce_view->GetViewAccessibility().GetCachedName(),
              u"Queued microphone alert");
  }
#endif

  // Once attached at least once, detaching the view off-widget (e.g. when
  // cached in `SidePanelEntry` while another side panel is open) drops alerts
  // instead of queuing a stale announcement for re-attachment.
  std::unique_ptr<views::View> cached_view =
      container->RemoveChildViewT(web_view);
  ASSERT_EQ(web_view->GetWidget(), nullptr);
  panel_host_->AnnounceAlert(u"Stale cached alert");
  EXPECT_EQ(web_view->pending_alert_for_testing(), std::nullopt);

  container->AddChildView(std::move(cached_view));
#if !BUILDFLAG(IS_MAC)
  if (!views::ViewAccessibility::IsViewsAccessibilityTreeEnabled()) {
    EXPECT_EQ(counter.GetCount(ax::mojom::Event::kAlert), 1);
  }
#endif

  widget.reset();
}

}  // namespace contextual_tasks
