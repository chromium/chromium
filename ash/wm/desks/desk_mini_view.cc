// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_mini_view.h"

#include <algorithm>

#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/close_desk_button.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

constexpr int kLabelPreviewSpacing = 8;

constexpr int kCloseButtonMargin = 8;

constexpr SkColor kActiveColor = SK_ColorWHITE;
constexpr SkColor kInactiveColor = SK_ColorTRANSPARENT;

constexpr SkColor kDraggedOverColor = SkColorSetARGB(0xFF, 0x5B, 0xBC, 0xFF);

std::unique_ptr<DeskPreviewView> CreateDeskPreviewView(
    DeskMiniView* mini_view) {
  auto desk_preview_view = std::make_unique<DeskPreviewView>(mini_view);
  desk_preview_view->set_owned_by_client();
  return desk_preview_view;
}

// Returns the width of the desk preview based on its |preview_height| and the
// aspect ratio of the root window taken from |root_window_size|.
int GetPreviewWidth(const gfx::Size& root_window_size, int preview_height) {
  return preview_height * root_window_size.width() / root_window_size.height();
}

// The desk preview bounds are proportional to the bounds of the display on
// which it resides, but always has a fixed height given as |preview_height|
// which depends on the width of the OverviewGrid.
gfx::Rect GetDeskPreviewBounds(aura::Window* root_window, int preview_height) {
  const auto root_size = root_window->GetBoundsInRootWindow().size();
  return gfx::Rect(GetPreviewWidth(root_size, preview_height), preview_height);
}

}  // namespace

// -----------------------------------------------------------------------------
// DeskMiniView

DeskMiniView::DeskMiniView(DesksBarView* owner_bar,
                           aura::Window* root_window,
                           Desk* desk,
                           const base::string16& title)
    : views::Button(owner_bar),
      owner_bar_(owner_bar),
      root_window_(root_window),
      desk_(desk),
      desk_preview_(CreateDeskPreviewView(this)),
      label_(new views::Label(title)),
      close_desk_button_(new CloseDeskButton(this)) {
  desk_->AddObserver(this);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetSubpixelRenderingEnabled(false);
  label_->set_can_process_events_within_subtree(false);
  label_->SetEnabledColor(SK_ColorWHITE);
  label_->SetLineHeight(10);

  close_desk_button_->SetVisible(false);

  // TODO(afakhry): Tooltips and accessible names.

  AddChildView(desk_preview_.get());
  AddChildView(label_);
  AddChildView(close_desk_button_);

  SetFocusPainter(nullptr);
  SetInkDropMode(InkDropMode::OFF);

  UpdateBorderColor();
  SetAccessibleName(title);
}

DeskMiniView::~DeskMiniView() {
  // In tests, where animations are disabled, the mini_view maybe destroyed
  // before the desk.
  if (desk_)
    desk_->RemoveObserver(this);
}

void DeskMiniView::SetTitle(const base::string16& title) {
  label_->SetText(title);
  SetAccessibleName(title);
}

aura::Window* DeskMiniView::GetDeskContainer() const {
  DCHECK(desk_);
  return desk_->GetDeskContainerForRoot(root_window_);
}

void DeskMiniView::OnHoverStateMayHaveChanged() {
  // Don't show the close button when hovered while the dragged window is on
  // the DesksBarView.
  close_desk_button_->SetVisible(
      DesksController::Get()->CanRemoveDesks() &&
      !owner_bar_->dragged_item_over_bar() &&
      (IsMouseHovered() || force_show_close_button_));
}

void DeskMiniView::OnWidgetGestureTap(const gfx::Rect& screen_rect,
                                      bool is_long_gesture) {
  const bool old_force_show_close_button = force_show_close_button_;
  // Note that we don't want to hide the close button if it's a single tap
  // within the bounds of an already visible button, which will later be handled
  // as a press event on that close button that will result in closing the desk.
  force_show_close_button_ =
      (is_long_gesture && IsPointOnMiniView(screen_rect.CenterPoint())) ||
      (!is_long_gesture && close_desk_button_->GetVisible() &&
       close_desk_button_->DoesIntersectScreenRect(screen_rect));
  if (old_force_show_close_button != force_show_close_button_)
    OnHoverStateMayHaveChanged();
}

void DeskMiniView::UpdateBorderColor() {
  DCHECK(desk_);
  if (owner_bar_->dragged_item_over_bar() &&
      IsPointOnMiniView(owner_bar_->last_dragged_item_screen_location())) {
    desk_preview_->SetBorderColor(kDraggedOverColor);
  } else if (IsViewHighlighted()) {
    desk_preview_->SetBorderColor(gfx::kGoogleBlue300);
  } else {
    desk_preview_->SetBorderColor(desk_->is_active() ? kActiveColor
                                                     : kInactiveColor);
  }
}

const char* DeskMiniView::GetClassName() const {
  return "DeskMiniView";
}

void DeskMiniView::Layout() {
  auto* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  DCHECK(root_window);

  const bool compact = owner_bar_->UsesCompactLayout();
  const gfx::Rect preview_bounds =
      GetDeskPreviewBounds(root_window, DeskPreviewView::GetHeight(compact));
  desk_preview_->SetBoundsRect(preview_bounds);

  label_->SetVisible(!compact);

  if (!compact) {
    const gfx::Size label_size = label_->GetPreferredSize();
    const gfx::Rect label_bounds{
        (preview_bounds.width() - label_size.width()) / 2,
        preview_bounds.bottom() + kLabelPreviewSpacing, label_size.width(),
        label_size.height()};
    label_->SetBoundsRect(label_bounds);
  }

  close_desk_button_->SetBounds(
      preview_bounds.right() - CloseDeskButton::kCloseButtonSize -
          kCloseButtonMargin,
      kCloseButtonMargin, CloseDeskButton::kCloseButtonSize,
      CloseDeskButton::kCloseButtonSize);

  Button::Layout();
}

gfx::Size DeskMiniView::CalculatePreferredSize() const {
  auto* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  DCHECK(root_window);

  const bool compact = owner_bar_->UsesCompactLayout();
  const gfx::Rect preview_bounds =
      GetDeskPreviewBounds(root_window, DeskPreviewView::GetHeight(compact));
  if (compact)
    return preview_bounds.size();

  // The preferred size takes into account only the width of the preview
  // view.
  return gfx::Size{preview_bounds.width(),
                   preview_bounds.height() + kLabelPreviewSpacing +
                       label_->GetPreferredSize().height()};
}

void DeskMiniView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::Button::GetAccessibleNodeData(node_data);

  // Note that the desk may have already been destroyed.
  if (desk_ && desk_->is_active()) {
    node_data->AddStringAttribute(
        ax::mojom::StringAttribute::kValue,
        l10n_util::GetStringUTF8(
            IDS_ASH_DESKS_ACTIVE_DESK_MINIVIEW_A11Y_EXTRA_TIP));
  }

  if (DesksController::Get()->CanRemoveDesks()) {
    node_data->AddStringAttribute(
        ax::mojom::StringAttribute::kDescription,
        l10n_util::GetStringUTF8(
            IDS_ASH_OVERVIEW_CLOSABLE_HIGHLIGHT_ITEM_A11Y_EXTRA_TIP));
  }
}

void DeskMiniView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  DCHECK(desk_);
  if (sender != close_desk_button_)
    return;

  OnCloseButtonPressed();
}

void DeskMiniView::OnContentChanged() {
  desk_preview_->RecreateDeskContentsMirrorLayers();
}

void DeskMiniView::OnDeskDestroyed(const Desk* desk) {
  // Note that the mini_view outlives the desk (which will be removed after all
  // DeskController's observers have been notified of its removal) because of
  // the animation.
  // Note that we can't make it the other way around (i.e. make the desk outlive
  // the mini_view). The desk's existence (or lack thereof) is more important
  // than the existence of the mini_view, since it determines whether we can
  // create new desks or remove existing ones. This determines whether the close
  // button will show on hover, and whether the new_desk_button is enabled. We
  // shouldn't allow that state to be wrong while the mini_views perform the
  // desk removal animation.
  // TODO(afakhry): Consider detaching the layer and destroying the mini_view
  // directly.

  DCHECK_EQ(desk_, desk);
  desk_ = nullptr;

  // No need to remove `this` as an observer; it's done automatically.
}

views::View* DeskMiniView::GetView() {
  return this;
}

gfx::Rect DeskMiniView::GetHighlightBoundsInScreen() {
  gfx::Rect bounds_in_screen = bounds();
  views::View::ConvertRectToScreen(this->parent(), &bounds_in_screen);
  return bounds_in_screen;
}

void DeskMiniView::MaybeActivateHighlightedView() {
  DesksController::Get()->ActivateDesk(desk(),
                                       DesksSwitchSource::kMiniViewButton);
}

void DeskMiniView::MaybeCloseHighlightedView() {
  OnCloseButtonPressed();
}

bool DeskMiniView::OnViewHighlighted() {
  UpdateBorderColor();
  return true;
}

void DeskMiniView::OnViewUnhighlighted() {
  UpdateBorderColor();
}

bool DeskMiniView::IsPointOnMiniView(const gfx::Point& screen_location) const {
  gfx::Point point_in_view = screen_location;
  ConvertPointFromScreen(this, &point_in_view);
  return HitTestPoint(point_in_view);
}

int DeskMiniView::GetMinWidthForDefaultLayout() const {
  auto* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  DCHECK(root_window);

  return GetPreviewWidth(root_window->GetBoundsInRootWindow().size(),
                         DeskPreviewView::GetHeight(/*compact=*/false));
}

void DeskMiniView::OnCloseButtonPressed() {
  auto* controller = DesksController::Get();
  if (!controller->CanRemoveDesks())
    return;

  // Hide the close button so it can no longer be pressed.
  close_desk_button_->SetVisible(false);

  // This mini_view can no longer be pressed.
  listener_ = nullptr;

  controller->RemoveDesk(desk_, DesksCreationRemovalSource::kButton);
}

}  // namespace ash
