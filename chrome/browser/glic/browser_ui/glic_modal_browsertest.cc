// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_modal.h"

#include <memory>
#include <utility>

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace glic {
namespace {

using GlicModalBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(GlicModalBrowserTest, ShowAndCloseModal) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  bool webui_created_called = false;
  base::test::TestFuture<void> closed_future;

  GlicModal::Params params;
  params.content_url = GURL(chrome::kChromeUIVersionURL);
  params.accessible_title = u"Test Modal Title";
  params.on_webui_created =
      base::BindLambdaForTesting([&](content::WebContents* contents) {
        ASSERT_TRUE(contents);
        webui_created_called = true;
      });
  params.on_closed = closed_future.GetCallback();

  views::test::AXEventCounter ax_counter(views::AXUpdateNotifier::Get());

  std::unique_ptr<GlicModal> modal = GlicModal::Show(*tab, std::move(params));
  ASSERT_TRUE(modal);
  EXPECT_TRUE(webui_created_called);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* tracked_view =
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          GlicModal::kDialogElementId,
          views::ElementTrackerViews::GetContextForWidget(
              browser_view->GetWidget()));
  ASSERT_TRUE(tracked_view);
  views::Widget* widget = tracked_view->GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(widget->widget_delegate()->GetAccessibleWindowTitle(),
            u"Test Modal Title");
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return ax_counter.GetCount(ax::mojom::Event::kAlert) >= 1; }));

  content::WebContents* modal_contents = modal->GetWebContents();
  ASSERT_TRUE(modal_contents);
  ASSERT_TRUE(content::WaitForLoadStop(modal_contents));

  modal->Close();
  EXPECT_TRUE(closed_future.Wait());
  EXPECT_EQ(modal->GetWebContents(), nullptr);
}

IN_PROC_BROWSER_TEST_F(GlicModalBrowserTest,
                       DestroyingModalDoesNotRunOnClosed) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  bool closed_called = false;
  GlicModal::Params params;
  params.content_url = GURL(chrome::kChromeUIVersionURL);
  params.on_closed =
      base::BindLambdaForTesting([&]() { closed_called = true; });

  std::unique_ptr<GlicModal> modal = GlicModal::Show(*tab, std::move(params));
  ASSERT_TRUE(modal);
  modal.reset();

  EXPECT_FALSE(closed_called);
}

IN_PROC_BROWSER_TEST_F(GlicModalBrowserTest,
                       ReturnsNullWhenTabCannotShowModalUI) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  GlicModal::Params params1;
  params1.content_url = GURL(chrome::kChromeUIVersionURL);
  std::unique_ptr<GlicModal> modal1 = GlicModal::Show(*tab, std::move(params1));
  ASSERT_TRUE(modal1);

  bool second_closed_called = false;
  GlicModal::Params params2;
  params2.content_url = GURL(chrome::kChromeUIVersionURL);
  params2.on_closed =
      base::BindLambdaForTesting([&]() { second_closed_called = true; });
  std::unique_ptr<GlicModal> modal2 = GlicModal::Show(*tab, std::move(params2));
  EXPECT_FALSE(modal2);
  EXPECT_FALSE(second_closed_called);
}

IN_PROC_BROWSER_TEST_F(GlicModalBrowserTest,
                       OnClosedCanDestroyModalReentrantly) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  std::unique_ptr<GlicModal> modal;
  bool closed_called = false;
  GlicModal::Params params;
  params.content_url = GURL(chrome::kChromeUIVersionURL);
  params.on_closed = base::BindLambdaForTesting([&]() {
    closed_called = true;
    modal.reset();
  });

  modal = GlicModal::Show(*tab, std::move(params));
  ASSERT_TRUE(modal);
  modal->Close();
  EXPECT_TRUE(closed_called);
  EXPECT_FALSE(modal);
}

IN_PROC_BROWSER_TEST_F(GlicModalBrowserTest, LoadingViewAndShowContent) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  gfx::Size received_available_size;
  GlicModal::Params params;
  params.content_url = GURL(chrome::kChromeUIVersionURL);
  params.on_available_size_changed = base::BindLambdaForTesting(
      [&](gfx::Size size) { received_available_size = size; });

  auto loading_view = std::make_unique<views::View>();
  views::View* raw_loading_view = loading_view.get();
  params.loading_view = std::move(loading_view);

  std::unique_ptr<GlicModal> modal = GlicModal::Show(*tab, std::move(params));
  ASSERT_TRUE(modal);

  // Available size callback should have been invoked with params.max_size.
  EXPECT_EQ(received_available_size, gfx::Size(512, 800));

  // Verify loading view is attached to dialog and has a ui::Layer with rounded
  // corners.
  views::ViewTracker loading_view_tracker(raw_loading_view);
  ASSERT_TRUE(loading_view_tracker.view());
  EXPECT_TRUE(raw_loading_view->GetWidget());
  ASSERT_TRUE(raw_loading_view->layer());
  const gfx::RoundedCornersF expected_radii(
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kDialogRadius));
  EXPECT_EQ(raw_loading_view->layer()->rounded_corner_radii(), expected_radii);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* web_view =
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          GlicModal::kDialogElementId,
          views::ElementTrackerViews::GetContextForWidget(
              browser_view->GetWidget()));
  ASSERT_TRUE(web_view);

  // Loading view was set as initially focused view.
  EXPECT_FALSE(web_view->HasFocus());

  content::WebContents* modal_contents = modal->GetWebContents();
  ASSERT_TRUE(modal_contents);
  ASSERT_TRUE(content::WaitForLoadStop(modal_contents));

  // Still not focused until ShowContent() is called.
  EXPECT_FALSE(web_view->HasFocus());

  // ShowContent() reveals content, removes the loading view, and focuses
  // web_view.
  modal->ShowContent();
  EXPECT_FALSE(loading_view_tracker.view());
  EXPECT_TRUE(web_view->HasFocus());

  // Calling ShowContent() again is a safe no-op.
  modal->ShowContent();
  EXPECT_TRUE(web_view->HasFocus());

  modal->Close();
}

IN_PROC_BROWSER_TEST_F(GlicModalBrowserTest, AvailableSizeChangedCallback) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  gfx::Size received_size;
  GlicModal::Params params;
  params.content_url = GURL(chrome::kChromeUIVersionURL);
  params.max_size = gfx::Size(600, 700);
  params.on_available_size_changed =
      base::BindLambdaForTesting([&](gfx::Size size) { received_size = size; });

  std::unique_ptr<GlicModal> modal = GlicModal::Show(*tab, std::move(params));
  ASSERT_TRUE(modal);
  EXPECT_EQ(received_size, gfx::Size(600, 700));

  modal->Close();
}

}  // namespace
}  // namespace glic
