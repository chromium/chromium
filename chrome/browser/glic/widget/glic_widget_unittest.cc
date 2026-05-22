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
TEST_F(GlicWidgetTest, DraggableRegionBypassedForOwnedWidgets) {
  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);
  GlicView* view_ptr = glic_view.get();

  auto delegate = GlicWidget::CreateWidgetDelegate(std::move(glic_view), true);
  auto widget =
      GlicWidget::Create(delegate.get(), profile(), gfx::Rect(10, 10, 800, 600),
                         true, GetContext());
  widget->Show();

  // Set a draggable region covering the entire view.
  std::vector<blink::mojom::DraggableRegionPtr> regions;
  auto region = blink::mojom::DraggableRegion::New();
  region->bounds = gfx::Rect(0, 0, 800, 600);
  region->draggable = true;
  regions.push_back(std::move(region));
  view_ptr->DraggableRegionsChanged(regions, nullptr);

  // Create an owned widget simulating a constrained dialog.
  auto owned_widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::CLIENT_OWNS_WIDGET;
  params.parent = widget->GetNativeView();
  params.bounds = gfx::Rect(50, 50, 200, 200);
  owned_widget->Init(std::move(params));
  owned_widget->Show();

  // A click inside the owned widget should now return true, bypassing the
  // draggable region.
  gfx::Point point_in_owned_screen(100, 100);
  gfx::Point point_in_owned_local = point_in_owned_screen;
  views::View::ConvertPointFromScreen(widget->GetRootView(),
                                      &point_in_owned_local);

  EXPECT_TRUE(widget->widget_delegate()->ShouldDescendIntoChildForEventHandling(
      owned_widget->GetNativeView(), point_in_owned_local));

  // A click outside the owned widget but inside the draggable region should
  // still return false.
  gfx::Point point_outside_screen(400, 400);
  gfx::Point point_outside_local = point_outside_screen;
  views::View::ConvertPointFromScreen(widget->GetRootView(),
                                      &point_outside_local);

  EXPECT_FALSE(
      widget->widget_delegate()->ShouldDescendIntoChildForEventHandling(
          owned_widget->GetNativeView(), point_outside_local));
}
#endif  // defined(USE_AURA)

}  // namespace glic
