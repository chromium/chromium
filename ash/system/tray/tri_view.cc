// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tri_view.h"

#include "ash/system/tray/size_range_layout.h"
#include "base/check.h"
#include "base/notreached.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace ash {
namespace {

// Converts TriView::Orientation values to views::BoxLayout::Orientation values.
views::BoxLayout::Orientation GetOrientation(TriView::Orientation orientation) {
  switch (orientation) {
    case TriView::Orientation::HORIZONTAL:
      return views::BoxLayout::Orientation::kHorizontal;
    case TriView::Orientation::VERTICAL:
      return views::BoxLayout::Orientation::kVertical;
  }
  // Required for some compilers.
  NOTREACHED();
}

}  // namespace

TriView::TriView() : TriView(0) {}

TriView::TriView(int padding_between_containers)
    : TriView(Orientation::HORIZONTAL, padding_between_containers) {}

TriView::TriView(Orientation orientation) : TriView(orientation, 0) {}

TriView::TriView(Orientation orientation, int padding_between_containers) {
  start_container_layout_manager_ = AddChildView(new SizeRangeLayout);
  center_container_layout_manager_ = AddChildView(new SizeRangeLayout);
  end_container_layout_manager_ = AddChildView(new SizeRangeLayout);

  auto layout = std::make_unique<views::BoxLayout>(
      GetOrientation(orientation), gfx::Insets(), padding_between_containers);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  box_layout_ = SetLayoutManager(std::move(layout));

  enable_hierarchy_changed_dcheck_ = true;
}

TriView::~TriView() {
  enable_hierarchy_changed_dcheck_ = false;
}

void TriView::SetMinHeight(int height) {
  gfx::Size min_size;

  min_size = GetMinSize(TriView::Container::START);
  min_size.set_height(height);
  SetMinSize(TriView::Container::START, min_size);

  min_size = GetMinSize(TriView::Container::CENTER);
  min_size.set_height(height);
  SetMinSize(TriView::Container::CENTER, min_size);

  min_size = GetMinSize(TriView::Container::END);
  min_size.set_height(height);
  SetMinSize(TriView::Container::END, min_size);
}

void TriView::SetMinSize(Container container, const gfx::Size& size) {
  GetLayoutManager(container)->SetMinSize(size);
}

gfx::Size TriView::GetMinSize(Container container) {
  return GetLayoutManager(container)->min_size();
}

void TriView::SetMaxSize(Container container, const gfx::Size& size) {
  GetLayoutManager(container)->SetMaxSize(size);
}

void TriView::AddView(Container container, views::View* view) {
  GetContainer(container)->AddChildView(view);
}

void TriView::AddViewAt(Container container, views::View* view, int index) {
  GetContainer(container)->AddChildViewAt(view, index);
}

void TriView::SetInsets(const gfx::Insets& insets) {
  box_layout_->set_inside_border_insets(insets);
}

void TriView::SetContainerBorder(Container container,
                                 std::unique_ptr<views::Border> border) {
  GetContainer(container)->SetBorder(std::move(border));
}

void TriView::SetContainerVisible(Container container, bool visible) {
  if (GetContainer(container)->GetVisible() == visible)
    return;
  GetContainer(container)->SetVisible(visible);
  DeprecatedLayoutImmediately();
}

void TriView::SetFlexForContainer(Container container, int flex) {
  box_layout_->SetFlexForView(GetContainer(container), flex);
}

void TriView::SetContainerLayout(
    Container container,
    std::unique_ptr<views::LayoutManager> layout_manager) {
  GetLayoutManager(container)->SetLayoutManager(std::move(layout_manager));
}

void TriView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  views::View::ViewHierarchyChanged(details);
  if (!enable_hierarchy_changed_dcheck_)
    return;

  if (details.parent == this) {
    if (details.is_add) {
      DCHECK(false)
          << "Child views should not be added directly. They should be added "
             "using TriView::AddView().";
    } else {
      DCHECK(false) << "Container views should not be removed.";
    }
  }
}

gfx::Rect TriView::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = View::GetAnchorBoundsInScreen();

  // Inset bounds a bit so that bubbles overlap the nominal empty space at
  // the bottom of the TriView slightly.
  // This specific piece of code was added to accommodate a specific refactoring
  // where anchor insets had to be removed from
  // NetworkStateListDetailedView::InfoBubble. This bubble is the only one I
  // could find that directly anchors directly to a TriView.
  // If there are other instantiations of TriView where this overlap doesn't
  // make sense, the below inset could be settable on TriView and called from
  // NetworkStateListDetailedView.
  bounds.Inset(gfx::Insets::TLBR(0, 0, 8, 0));
  return bounds;
}

views::View* TriView::GetContainer(Container container) {
  return children()[static_cast<size_t>(container)];
}

SizeRangeLayout* TriView::GetLayoutManager(Container container) {
  switch (container) {
    case Container::START:
      return start_container_layout_manager_;
    case Container::CENTER:
      return center_container_layout_manager_;
    case Container::END:
      return end_container_layout_manager_;
  }
  // Required for some compilers.
  NOTREACHED();
}

BEGIN_METADATA(TriView)
END_METADATA

}  // namespace ash
