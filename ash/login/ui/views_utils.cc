// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/views_utils.h"

#include <memory>

#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/style/ash_color_provider.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class ContainerView : public NonAccessibleView,
                      public views::ViewTargeterDelegate {
  METADATA_HEADER(ContainerView, NonAccessibleView)

 public:
  ContainerView() {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  }

  ContainerView(const ContainerView&) = delete;
  ContainerView& operator=(const ContainerView&) = delete;

  ~ContainerView() override = default;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    const auto& children = target->children();
    const auto hits_child = [target, rect](const views::View* child) {
      gfx::RectF child_rect(rect);
      views::View::ConvertRectToTarget(target, child, &child_rect);
      return child->GetVisible() &&
             child->HitTestRect(gfx::ToEnclosingRect(child_rect));
    };
    return base::ranges::any_of(children, hits_child);
  }
};

BEGIN_METADATA(ContainerView)
END_METADATA

}  // namespace

namespace login_views_utils {

std::unique_ptr<views::View> WrapViewForPreferredSize(
    std::unique_ptr<views::View> view) {
  // Using ContainerView here ensures that click events will be passed to the
  // wrapped view even if a transform is applied that moves the view outside the
  // wrapper.
  auto proxy = std::make_unique<ContainerView>();
  auto layout_manager = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  proxy->SetLayoutManager(std::move(layout_manager));
  proxy->AddChildView(std::move(view));
  return proxy;
}

bool ShouldShowLandscape(const views::Widget* widget) {
  // |widget| is null when the view is being constructed. Default to landscape
  // in that case. A new layout will happen when the view is attached to a
  // widget (see LockContentsView::AddedToWidget), which will let us fetch the
  // correct display orientation.
  if (!widget) {
    return true;
  }

  // Get the orientation for |widget|.
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());

  // The display bounds are updated after a rotation. This means that if the
  // device has resolution 800x600, and the rotation is
  // display::Display::ROTATE_0, bounds() is 800x600. On
  // display::Display::ROTATE_90, bounds() is 600x800.
  //
  // ash/login/ui assumes landscape means width>height, and portrait means
  // height>width.
  //
  // Considering the actual rotation of the device introduces edge-cases, ie,
  // when the device resolution in display::Display::ROTATE_0 is 768x1024, such
  // as in https://crbug.com/858858.
  return display.bounds().width() > display.bounds().height();
}

bool HasFocusInAnyChildView(views::View* view) {
  CHECK(view);
  views::FocusManager* focus_manager = view->GetFocusManager();
  CHECK(focus_manager);

  views::View* focused_view = focus_manager->GetFocusedView();
  if (focused_view) {
    return view->Contains(focused_view);
  } else {
    return false;
  }
}

std::unique_ptr<views::Label> CreateUnthemedBubbleLabel(
    const std::u16string& message,
    views::View* view_defining_max_width,
    const gfx::FontList& font_list,
    int line_height) {
  auto builder = views::Builder<views::Label>()
                     .SetText(message)
                     .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                     .SetAutoColorReadabilityEnabled(false)
                     .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                     .SetSubpixelRenderingEnabled(false)
                     .SetFontList(font_list)
                     .SetLineHeight(line_height);
  if (view_defining_max_width != nullptr) {
    builder.SetMultiLine(true)
        .SetAllowCharacterBreak(true)
        // Make sure to set a maximum label width, otherwise text wrapping will
        // significantly increase width and layout may not work correctly if
        // the input string is very long.
        .SetMaximumWidth(view_defining_max_width->GetPreferredSize().width());
  }
  return std::move(builder).Build();
}

std::unique_ptr<views::Label> CreateBubbleLabel(
    const std::u16string& message,
    views::View* view_defining_max_width,
    SkColor color,
    const gfx::FontList& font_list,
    int line_height) {
  auto label = CreateUnthemedBubbleLabel(message, view_defining_max_width,
                                         font_list, line_height);
  label->SetEnabledColor(color);
  return label;
}

std::unique_ptr<views::Label> CreateThemedBubbleLabel(
    const std::u16string& message,
    views::View* view_defining_max_width,
    ui::ColorId enabled_color_type,
    const gfx::FontList& font_list,
    int line_height) {
  auto label = CreateUnthemedBubbleLabel(message, view_defining_max_width,
                                         font_list, line_height);
  label->SetEnabledColorId(enabled_color_type);
  return label;
}

views::View* GetBubbleContainer(views::View* view) {
  views::View* v = view;
  while (v->parent() != nullptr) {
    v = v->parent();
  }

  views::View* root_view = v;
  // An arbitrary id that no other child of root view should use.
  const int kMenuContainerId = 1000;
  views::View* container = nullptr;
  for (views::View* child : root_view->children()) {
    if (child->GetID() == kMenuContainerId) {
      container = child;
      break;
    }
  }

  if (!container) {
    container = root_view->AddChildView(std::make_unique<ContainerView>());
    container->SetID(kMenuContainerId);
  }

  return container;
}

gfx::Point CalculateBubblePositionBeforeAfterStrategy(gfx::Rect anchor,
                                                      gfx::Size bubble,
                                                      gfx::Rect bounds) {
  gfx::Rect result(anchor.x() - bubble.width(), anchor.y(), bubble.width(),
                   bubble.height());
  // Trying to show before (on the left side in LTR).
  // If there is not enough space show after (on the right side in LTR).
  if (result.x() < bounds.x()) {
    result.Offset(anchor.width() + result.width(), 0);
  }
  result.AdjustToFit(bounds);
  return result.origin();
}

gfx::Point CalculateBubblePositionAfterBeforeStrategy(gfx::Rect anchor,
                                                      gfx::Size bubble,
                                                      gfx::Rect bounds) {
  gfx::Rect result(anchor.x() + anchor.width(), anchor.y(), bubble.width(),
                   bubble.height());
  // Trying to show after (on the right side in LTR).
  // If there is not enough space show before (on the left side in LTR).
  if (result.right() > bounds.right()) {
    result.Offset(-anchor.width() - result.width(), 0);
  }
  result.AdjustToFit(bounds);
  return result.origin();
}

void ConfigureRectFocusRingCircleInkDrop(views::View* view,
                                         views::FocusRing* focus_ring,
                                         std::optional<int> radius) {
  DCHECK(view);
  DCHECK(focus_ring);
  focus_ring->SetPathGenerator(
      std::make_unique<views::RectHighlightPathGenerator>());

  if (radius) {
    views::InstallFixedSizeCircleHighlightPathGenerator(view, *radius);
  } else {
    views::InstallCircleHighlightPathGenerator(view);
  }
}

}  // namespace login_views_utils
}  // namespace ash
