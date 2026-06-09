// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_widget.h"

#include <memory>

#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace glic {

class GlicWidgetTest : public ChromeViewsTestBase {
 public:
  GlicWidgetTest() = default;
  ~GlicWidgetTest() override = default;

  TestingProfile* profile() { return &profile_; }

 private:
  TestingProfile profile_;
};

#if defined(USE_AURA)
TEST_F(GlicWidgetTest, EventHandlingHitTestRules) {
  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);
  GlicView* view_ptr = glic_view.get();

  auto delegate = GlicWidget::CreateWidgetDelegate(std::move(glic_view), true);
  auto widget =
      GlicWidget::Create(delegate.get(), profile(), gfx::Rect(10, 10, 800, 600),
                         true, GetContext());
  widget->Show();

  // Create a child view (representing the webview).
  widget->GetContentsView()->AddChildView(std::make_unique<views::View>());

  // Set a draggable region covering the top 50 pixels.
  std::vector<blink::mojom::DraggableRegionPtr> regions;
  auto region = blink::mojom::DraggableRegion::New();
  region->bounds = gfx::Rect(0, 0, 800, 50);
  region->draggable = true;
  regions.push_back(std::move(region));
  view_ptr->DraggableRegionsChanged(regions, nullptr);

  // 1. Client area (not draggable, not a border)
  gfx::Point client_point_screen =
      widget->GetWindowBoundsInScreen().CenterPoint();
  gfx::Point client_point_local = client_point_screen;
  views::View::ConvertPointFromScreen(widget->GetRootView(),
                                      &client_point_local);

  int hit_test = widget->GetNonClientComponent(client_point_local);
  if (hit_test == HTCLIENT || hit_test == HTNOWHERE) {
    EXPECT_TRUE(
        widget->widget_delegate()->ShouldDescendIntoChildForEventHandling(
            widget->GetNativeView(), client_point_local));
  }

  // 2. Draggable region (acts as caption)
  gfx::Point draggable_point_local(100, 25);  // Inside the 50px top region
  EXPECT_FALSE(
      widget->widget_delegate()->ShouldDescendIntoChildForEventHandling(
          widget->GetNativeView(), draggable_point_local));

  // 3. Owned widget (simulating constrained dialog)
  auto owned_widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::CLIENT_OWNS_WIDGET;
  params.parent = widget->GetNativeView();
  params.bounds = gfx::Rect(50, 10, 200, 200);  // Overlaps draggable region
  owned_widget->Init(std::move(params));
  owned_widget->Show();

  gfx::Point owned_point_local(
      100, 25);  // Overlaps draggable region but inside owned widget
  // It should descend into child (owned widget handles it).
  EXPECT_TRUE(widget->widget_delegate()->ShouldDescendIntoChildForEventHandling(
      owned_widget->GetNativeView(), owned_point_local));

  // 4. Resize border
  gfx::Rect window_bounds = widget->GetWindowBoundsInScreen();
  gfx::Point bottom_border(window_bounds.CenterPoint().x(),
                           window_bounds.bottom() - 1);
  gfx::Point bottom_border_local = bottom_border;
  views::View::ConvertPointFromScreen(widget->GetRootView(),
                                      &bottom_border_local);

  hit_test = widget->GetNonClientComponent(bottom_border_local);
  EXPECT_FALSE(
      widget->widget_delegate()->ShouldDescendIntoChildForEventHandling(
          widget->GetNativeView(), bottom_border_local));
}
#endif  // defined(USE_AURA)

}  // namespace glic
