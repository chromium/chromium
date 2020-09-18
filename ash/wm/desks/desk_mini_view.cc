// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_mini_view.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/close_desk_button.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

constexpr int kLabelPreviewSpacing = 8;

constexpr int kCloseButtonMargin = 8;

constexpr int kMinDeskNameViewWidth = 20;

constexpr SkColor kDarkModeActiveColor = SK_ColorWHITE;
constexpr SkColor kLightModeActiveColor = SK_ColorBLACK;
constexpr SkColor kInactiveColor = SK_ColorTRANSPARENT;

constexpr SkColor kDraggedOverColor = SkColorSetARGB(0xFF, 0x5B, 0xBC, 0xFF);

// Returns the width of the desk preview based on its |preview_height| and the
// aspect ratio of the root window taken from |root_window_size|.
int GetPreviewWidth(const gfx::Size& root_window_size, int preview_height) {
  return preview_height * root_window_size.width() / root_window_size.height();
}

// The desk preview bounds are proportional to the bounds of the display on
// which it resides, and whether the |compact| layout is used.
gfx::Rect GetDeskPreviewBounds(aura::Window* root_window, bool compact) {
  const int preview_height = DeskPreviewView::GetHeight(root_window, compact);
  const auto root_size = root_window->bounds().size();
  return gfx::Rect(GetPreviewWidth(root_size, preview_height), preview_height);
}

}  // namespace

// -----------------------------------------------------------------------------
// DeskMiniView

DeskMiniView::DeskMiniView(DesksBarView* owner_bar,
                           aura::Window* root_window,
                           Desk* desk)
    : owner_bar_(owner_bar), root_window_(root_window), desk_(desk) {
  DCHECK(root_window_);
  DCHECK(root_window_->IsRootWindow());

  desk_->AddObserver(this);

  auto desk_name_view = std::make_unique<DeskNameView>();
  desk_name_view->AddObserver(this);
  desk_name_view->set_controller(this);
  desk_name_view->SetText(desk_->name());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // TODO(afakhry): Tooltips.

  desk_preview_ = AddChildView(std::make_unique<DeskPreviewView>(this));
  desk_name_view_ = AddChildView(std::move(desk_name_view));
  close_desk_button_ = AddChildView(std::make_unique<CloseDeskButton>(this));

  UpdateCloseButtonVisibility();
  UpdateBorderColor();
}

DeskMiniView::~DeskMiniView() {
  desk_name_view_->RemoveObserver(this);
  // In tests, where animations are disabled, the mini_view maybe destroyed
  // before the desk.
  if (desk_)
    desk_->RemoveObserver(this);
}

aura::Window* DeskMiniView::GetDeskContainer() const {
  DCHECK(desk_);
  return desk_->GetDeskContainerForRoot(root_window_);
}

bool DeskMiniView::IsDeskNameBeingModified() const {
  return desk_name_view_->HasFocus();
}

void DeskMiniView::UpdateCloseButtonVisibility() {
  // Don't show the close button when hovered while the dragged window is on
  // the DesksBarView.
  // For switch access, setting the close button to visible allows users to
  // navigate to it.
  close_desk_button_->SetVisible(
      DesksController::Get()->CanRemoveDesks() &&
      !owner_bar_->dragged_item_over_bar() &&
      (IsMouseHovered() || force_show_close_button_ ||
       Shell::Get()->accessibility_controller()->IsSwitchAccessRunning()));
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
    UpdateCloseButtonVisibility();
}

void DeskMiniView::UpdateBorderColor() {
  DCHECK(desk_);
  auto* color_provider = AshColorProvider::Get();
  if (owner_bar_->dragged_item_over_bar() &&
      IsPointOnMiniView(owner_bar_->last_dragged_item_screen_location())) {
    desk_preview_->SetBorderColor(kDraggedOverColor);
  } else if (IsViewHighlighted()) {
    desk_preview_->SetBorderColor(color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kFocusRingColor));
  } else if (!desk_->is_active()) {
    desk_preview_->SetBorderColor(kInactiveColor);
  } else {
    // Default theme for desks is dark mode.
    desk_preview_->SetBorderColor(color_provider->IsDarkModeEnabled()
                                      ? kDarkModeActiveColor
                                      : kLightModeActiveColor);
  }
}

const char* DeskMiniView::GetClassName() const {
  return "DeskMiniView";
}

void DeskMiniView::Layout() {
  const bool compact = owner_bar_->UsesCompactLayout();
  const gfx::Rect preview_bounds = GetDeskPreviewBounds(root_window_, compact);
  desk_preview_->SetBoundsRect(preview_bounds);

  desk_name_view_->SetVisible(!compact);

  if (!compact)
    LayoutDeskNameView(preview_bounds);

  close_desk_button_->SetBounds(
      preview_bounds.right() - CloseDeskButton::kCloseButtonSize -
          kCloseButtonMargin,
      kCloseButtonMargin, CloseDeskButton::kCloseButtonSize,
      CloseDeskButton::kCloseButtonSize);
}

gfx::Size DeskMiniView::CalculatePreferredSize() const {
  const bool compact = owner_bar_->UsesCompactLayout();
  const gfx::Rect preview_bounds = GetDeskPreviewBounds(root_window_, compact);
  if (compact)
    return preview_bounds.size();

  // The preferred size takes into account only the width of the preview
  // view.
  return gfx::Size{preview_bounds.width(),
                   preview_bounds.height() + 2 * kLabelPreviewSpacing +
                       desk_name_view_->GetPreferredSize().height()};
}

void DeskMiniView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  desk_preview_->GetAccessibleNodeData(node_data);

  // Note that the desk may have already been destroyed.
  if (desk_) {
    // Announce desk name.
    node_data->AddStringAttribute(
        ax::mojom::StringAttribute::kName,
        l10n_util::GetStringFUTF8(IDS_ASH_DESKS_DESK_ACCESSIBLE_NAME,
                                  desk_->name()));

    node_data->AddStringAttribute(
        ax::mojom::StringAttribute::kValue,
        l10n_util::GetStringUTF8(
            desk_->is_active()
                ? IDS_ASH_DESKS_ACTIVE_DESK_MINIVIEW_A11Y_EXTRA_TIP
                : IDS_ASH_DESKS_INACTIVE_DESK_MINIVIEW_A11Y_EXTRA_TIP));
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
  if (sender == close_desk_button_)
    OnCloseButtonPressed();
  else if (sender == desk_preview_)
    OnDeskPreviewPressed();
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

void DeskMiniView::OnDeskNameChanged(const base::string16& new_name) {
  if (is_desk_name_being_modified_)
    return;

  desk_name_view_->SetTextAndElideIfNeeded(new_name);
  desk_preview_->SetAccessibleName(new_name);

  Layout();
}

views::View* DeskMiniView::GetView() {
  return this;
}

void DeskMiniView::MaybeActivateHighlightedView() {
  DesksController::Get()->ActivateDesk(desk(),
                                       DesksSwitchSource::kMiniViewButton);
}

void DeskMiniView::MaybeCloseHighlightedView() {
  OnCloseButtonPressed();
}

void DeskMiniView::OnViewHighlighted() {
  UpdateBorderColor();
}

void DeskMiniView::OnViewUnhighlighted() {
  UpdateBorderColor();
}

void DeskMiniView::ContentsChanged(views::Textfield* sender,
                                   const base::string16& new_contents) {
  DCHECK_EQ(sender, desk_name_view_);
  DCHECK(is_desk_name_being_modified_);
  if (!desk_)
    return;

  // Avoid copying new_contents if we don't need to trim it below.
  const base::string16* new_text = &new_contents;

  // To avoid potential security and memory issues, we don't allow desk names to
  // have an unbounded length. Therefore we trim if needed at kMaxLength UTF-16
  // boundary. Note that we don't care about code point boundaries in this case.
  base::string16 trimmed_new_contents;
  if (new_contents.size() > DeskNameView::kMaxLength) {
    trimmed_new_contents = new_contents;
    trimmed_new_contents.resize(DeskNameView::kMaxLength);
    new_text = &trimmed_new_contents;
    desk_name_view_->SetText(trimmed_new_contents);
  }

  desk_->SetName(
      base::CollapseWhitespace(*new_text,
                               /*trim_sequences_with_line_breaks=*/false),
      /*set_by_user=*/true);

  Layout();
}

bool DeskMiniView::HandleKeyEvent(views::Textfield* sender,
                                  const ui::KeyEvent& key_event) {
  DCHECK_EQ(sender, desk_name_view_);
  DCHECK(is_desk_name_being_modified_);

  // Pressing enter or escape should blur the focus away from DeskNameView so
  // that editing the desk's name ends.
  if (key_event.type() != ui::ET_KEY_PRESSED)
    return false;

  if (key_event.key_code() != ui::VKEY_RETURN &&
      key_event.key_code() != ui::VKEY_ESCAPE) {
    return false;
  }

  DeskNameView::CommitChanges(GetWidget());

  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_DESKS_DESK_NAME_COMMIT, desk_->name()));
  return true;
}

bool DeskMiniView::HandleMouseEvent(views::Textfield* sender,
                                    const ui::MouseEvent& mouse_event) {
  DCHECK_EQ(sender, desk_name_view_);

  switch (mouse_event.type()) {
    case ui::ET_MOUSE_PRESSED:
      // If this is the first mouse press on the DeskNameView, then it's not
      // focused yet. OnViewFocused() should not select all text, since it will
      // be undone by the mouse release event. Instead we defer it until we get
      // the mouse release event.
      if (!is_desk_name_being_modified_)
        defer_select_all_ = true;
      break;

    case ui::ET_MOUSE_RELEASED:
      if (defer_select_all_) {
        defer_select_all_ = false;
        // The user may have already clicked and dragged to select some range
        // other than all the text. In this case, don't mess with an existing
        // selection.
        if (!desk_name_view_->HasSelection())
          desk_name_view_->SelectAll(false);
        return true;
      }
      break;

    default:
      break;
  }

  return false;
}

void DeskMiniView::OnViewFocused(views::View* observed_view) {
  DCHECK_EQ(observed_view, desk_name_view_);
  is_desk_name_being_modified_ = true;
  desk_name_view_->UpdateViewAppearance();

  // Set the unelided desk name so that the full name shows up for the user to
  // be able to change it.
  desk_name_view_->SetText(desk_->name());

  if (!defer_select_all_)
    desk_name_view_->SelectAll(false);
}

void DeskMiniView::OnViewBlurred(views::View* observed_view) {
  DCHECK_EQ(observed_view, desk_name_view_);
  is_desk_name_being_modified_ = false;
  defer_select_all_ = false;
  desk_name_view_->UpdateViewAppearance();

  // When committing the name, do not allow an empty desk name. Revert back to
  // the default name.
  // TODO(afakhry): Make this more robust. What if user renames a previously
  // user-modified desk name, say from "code" to "Desk 2", and that desk
  // happened to be in the second position. Since the new name matches the
  // default one for this position, should we revert it (i.e. consider it
  // `set_by_user = false`?
  if (desk_->name().empty()) {
    DesksController::Get()->RevertDeskNameToDefault(desk_);
    return;
  }

  OnDeskNameChanged(desk_->name());

  // Only when the new desk name has been committed is when we can update the
  // desks restore prefs.
  desks_restore_util::UpdatePrimaryUserDesksPrefs();
}

bool DeskMiniView::IsPointOnMiniView(const gfx::Point& screen_location) const {
  gfx::Point point_in_view = screen_location;
  ConvertPointFromScreen(this, &point_in_view);
  return HitTestPoint(point_in_view);
}

int DeskMiniView::GetMinWidthForDefaultLayout() const {
  const auto& root_size = root_window_->bounds().size();
  return GetPreviewWidth(root_size,
                         DeskPreviewView::GetHeight(root_window_,
                                                    /*compact=*/false));
}

bool DeskMiniView::IsDeskNameViewVisibleForTesting() const {
  return desk_name_view_->GetVisible();
}

void DeskMiniView::OnCloseButtonPressed() {
  auto* controller = DesksController::Get();
  if (!controller->CanRemoveDesks())
    return;

  // Hide the close button so it can no longer be pressed.
  close_desk_button_->SetVisible(false);

  desk_preview_->OnRemovingDesk();

  controller->RemoveDesk(desk_, DesksCreationRemovalSource::kButton);
}

void DeskMiniView::OnDeskPreviewPressed() {
  DesksController::Get()->ActivateDesk(desk_,
                                       DesksSwitchSource::kMiniViewButton);
}

void DeskMiniView::LayoutDeskNameView(const gfx::Rect& preview_bounds) {
  const int previous_width = desk_name_view_->width();
  const gfx::Size desk_name_view_size = desk_name_view_->GetPreferredSize();

  const int text_width =
      base::ClampToRange(desk_name_view_size.width(), kMinDeskNameViewWidth,
                         preview_bounds.width());

  const int desk_name_view_x =
      preview_bounds.x() + (preview_bounds.width() - text_width) / 2;
  gfx::Rect desk_name_view_bounds{
      desk_name_view_x, preview_bounds.bottom() + kLabelPreviewSpacing,
      text_width, desk_name_view_size.height()};
  desk_name_view_->SetBoundsRect(desk_name_view_bounds);

  // A change in the DeskNameView's width might mean the need
  // to elide the text differently.
  if (previous_width != desk_name_view_bounds.width())
    OnDeskNameChanged(desk_->name());
}

}  // namespace ash
