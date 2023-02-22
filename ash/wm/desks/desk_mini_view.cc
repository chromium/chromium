// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_mini_view.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/close_button.h"
#include "ash/style/style_util.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_action_context_menu.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/desks_textfield.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/cxx17_backports.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kLabelPreviewSpacing = 8;

constexpr int kCloseButtonMargin = 4;

constexpr int kMinDeskNameViewWidth = 56;

constexpr int kPreviewFocusRingRadiusOld = 6;

// TODO(conniekxu): After CrOS Next is launched, remove
// `kPreviewFocusRingRadiusOld`.
constexpr int kPreviewFocusRingRadius = 10;

gfx::Rect ConvertScreenRect(views::View* view, const gfx::Rect& screen_rect) {
  gfx::Point origin = screen_rect.origin();
  views::View::ConvertPointFromScreen(view, &origin);
  return gfx::Rect(origin, screen_rect.size());
}

// Tells whether `desk` contains an app window itself or if at least one visible
// on all desk window exists. Returns false if `desk` is nullptr.
bool ContainsAppWindows(Desk* desk) {
  if (!desk)
    return false;
  return desk->ContainsAppWindows() ||
         !DesksController::Get()->visible_on_all_desks_windows().empty();
}

}  // namespace

// -----------------------------------------------------------------------------
// DeskMiniView

// static
int DeskMiniView::GetPreviewWidth(const gfx::Size& root_window_size,
                                  int preview_height) {
  return preview_height * root_window_size.width() / root_window_size.height();
}

// static
gfx::Rect DeskMiniView::GetDeskPreviewBounds(aura::Window* root_window) {
  const int preview_height = DeskPreviewView::GetHeight(root_window);
  const auto root_size = root_window->bounds().size();
  return gfx::Rect(GetPreviewWidth(root_size, preview_height), preview_height);
}

DeskMiniView::DeskMiniView(DesksBarView* owner_bar,
                           aura::Window* root_window,
                           Desk* desk)
    : owner_bar_(owner_bar), root_window_(root_window), desk_(desk) {
  DCHECK(root_window_);
  DCHECK(root_window_->IsRootWindow());

  desk_->AddObserver(this);

  auto desk_name_view = std::make_unique<DeskNameView>(this);
  desk_name_view->AddObserver(this);
  desk_name_view->set_controller(this);
  desk_name_view->SetText(desk_->name());

  // Desks created by the new desk button are initialized with an empty name to
  // encourage user to name the desk, but the `desk_name_view` needs a non-empty
  // accessible name.
  auto* desks_controller = DesksController::Get();
  desk_name_view->SetAccessibleName(
      desk_->name().empty() ? DesksController::GetDeskDefaultName(
                                  desks_controller->GetDeskIndex(desk_))
                            : desk_->name());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // TODO(afakhry): Tooltips.

  desk_preview_ = AddChildView(std::make_unique<DeskPreviewView>(
      base::BindRepeating(&DeskMiniView::OnDeskPreviewPressed,
                          base::Unretained(this)),
      this));

  views::FocusRing* preview_focus_ring = views::FocusRing::Get(desk_preview_);
  views::InstallRoundRectHighlightPathGenerator(
      desk_preview_, gfx::Insets(kFocusRingHaloInset),
      chromeos::features::IsJellyrollEnabled() ? kPreviewFocusRingRadius
                                               : kPreviewFocusRingRadiusOld);

  preview_focus_ring->SetHasFocusPredicate([&](views::View* view) {
    return (owner_bar_->dragged_item_over_bar() &&
            IsPointOnMiniView(
                owner_bar_->last_dragged_item_screen_location())) ||
           desk_preview_->IsViewHighlighted() ||
           (desk_ && desk_->is_active() &&
            !owner_bar_->overview_grid()->IsShowingSavedDeskLibrary());
  });

  desk_name_view_ = AddChildView(std::move(desk_name_view));

  const std::u16string initial_combine_desks_target_name =
      desks_controller->GetCombineDesksTargetName(desk_);

  desk_action_view_ = AddChildView(std::make_unique<DeskActionView>(
      initial_combine_desks_target_name,
      /*combine_desks_callback=*/
      base::BindRepeating(&DeskMiniView::OnRemovingDesk, base::Unretained(this),
                          DeskCloseType::kCombineDesks),
      /*close_all_callback=*/
      base::BindRepeating(&DeskMiniView::OnRemovingDesk, base::Unretained(this),
                          DeskCloseType::kCloseAllWindowsAndWait)));

  context_menu_ = std::make_unique<DeskActionContextMenu>(
      initial_combine_desks_target_name,
      /*combine_desks_callback=*/
      base::BindRepeating(&DeskMiniView::OnRemovingDesk, base::Unretained(this),
                          DeskCloseType::kCombineDesks),
      /*close_all_callback=*/
      base::BindRepeating(&DeskMiniView::OnRemovingDesk, base::Unretained(this),
                          DeskCloseType::kCloseAllWindowsAndWait),
      base::BindRepeating(&DeskMiniView::OnContextMenuClosed,
                          base::Unretained(this)));
  UpdateDeskButtonVisibility();
}

DeskMiniView::~DeskMiniView() {
  desk_name_view_->RemoveObserver(this);
  // In tests, where animations are disabled, the mini_view maybe destroyed
  // before the desk.
  if (desk_)
    desk_->RemoveObserver(this);
}

gfx::Rect DeskMiniView::GetPreviewBoundsInScreen() const {
  DCHECK(desk_preview_);
  return desk_preview_->GetBoundsInScreen();
}

aura::Window* DeskMiniView::GetDeskContainer() const {
  DCHECK(desk_);
  return desk_->GetDeskContainerForRoot(root_window_);
}

bool DeskMiniView::IsDeskNameBeingModified() const {
  return desk_name_view_->HasFocus();
}

void DeskMiniView::UpdateDeskButtonVisibility() {
  auto* controller = DesksController::Get();

  // Don't show desk buttons when hovered while the dragged window is on
  // the DesksBarView.
  // For switch access, setting desk buttons to visible allows users to
  // navigate to it.
  const bool visible =
      controller->CanRemoveDesks() && !owner_bar_->dragged_item_over_bar() &&
      !owner_bar_->IsDraggingDesk() &&
      (IsMouseHovered() || force_show_desk_buttons_ ||
       Shell::Get()->accessibility_controller()->IsSwitchAccessRunning());

  // Only show the combine desks button if there are app windows in the desk,
  // or if the desk is active and there are windows that should be visible on
  // all desks.
  desk_action_view_->SetCombineDesksButtonVisibility(ContainsAppWindows(desk_));
  desk_action_view_->SetVisible(visible && !is_context_menu_open_);
}

void DeskMiniView::OnWidgetGestureTap(const gfx::Rect& screen_rect,
                                      bool is_long_gesture) {
  // Note that we don't want to hide the desk buttons if it's a single tap
  // within the bounds of an already visible button, which will later be
  // handled as a press event on that desk buttons that will result in closing
  // the desk.
  const bool old_force_show_desk_buttons = force_show_desk_buttons_;
  force_show_desk_buttons_ =
      !Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      ((is_long_gesture && IsPointOnMiniView(screen_rect.CenterPoint())) ||
       (!is_long_gesture && desk_action_view_->GetVisible() &&
        desk_action_view_->HitTestRect(
            ConvertScreenRect(desk_action_view_, screen_rect))));

  if (old_force_show_desk_buttons != force_show_desk_buttons_)
    UpdateDeskButtonVisibility();
}

void DeskMiniView::UpdateFocusColor() {
  DCHECK(desk_);
  absl::optional<ui::ColorId> new_focus_color_id;

  if ((owner_bar_->dragged_item_over_bar() &&
       IsPointOnMiniView(owner_bar_->last_dragged_item_screen_location())) ||
      desk_preview_->IsViewHighlighted()) {
    new_focus_color_id = ui::kColorAshFocusRing;
  } else if (desk_->is_active() &&
             !owner_bar_->overview_grid()->IsShowingSavedDeskLibrary()) {
    new_focus_color_id = kColorAshCurrentDeskColor;
  } else {
    new_focus_color_id = absl::nullopt;
  }

  if (desk_preview_->focus_color_id() == new_focus_color_id)
    return;

  auto* focus_ring = views::FocusRing::Get(desk_preview_);

  // Only repaint the focus ring if the color gets updated.
  desk_preview_->set_focus_color_id(new_focus_color_id);
  focus_ring->SetColorId(new_focus_color_id);

  focus_ring->SchedulePaint();
}

gfx::Insets DeskMiniView::GetPreviewBorderInsets() const {
  return desk_preview_->GetInsets();
}

bool DeskMiniView::IsPointOnMiniView(const gfx::Point& screen_location) const {
  gfx::Point point_in_view = screen_location;
  ConvertPointFromScreen(this, &point_in_view);
  // `this` doesn't have access to its widget until it's added to the view's
  // hierarchy, however this function could be triggered during the constructor
  // of `DeskMiniView` when it's not added to the view's hierarchy yet. Thus we
  // need to check whether the widget if accessible here.
  return GetWidget() && HitTestPoint(point_in_view);
}

void DeskMiniView::OpenContextMenu(ui::MenuSourceType source) {
  is_context_menu_open_ = true;
  UpdateDeskButtonVisibility();

  desk_preview_->SetHighlightOverlayVisibility(true);
  context_menu_->UpdateCombineDesksTargetName(
      DesksController::Get()->GetCombineDesksTargetName(desk_));

  // Only show the combine desks context menu option if there are app windows in
  // the desk, or if there are windows that should be visible on all desks.
  context_menu_->SetCombineDesksMenuItemVisibility(ContainsAppWindows(desk_));

  context_menu_->ShowContextMenuForView(
      this,
      base::i18n::IsRTL() ? desk_preview_->GetBoundsInScreen().bottom_right()
                          : desk_preview_->GetBoundsInScreen().bottom_left(),
      source);
}

void DeskMiniView::MaybeCloseContextMenu() {
  if (context_menu_)
    context_menu_->MaybeCloseMenu();
}

void DeskMiniView::OnRemovingDesk(DeskCloseType close_type) {
  if (!desk_)
    return;

  auto* controller = DesksController::Get();
  if (!controller->CanRemoveDesks())
    return;

  // We want to avoid the possibility of getting triggered multiple times. We
  // therefore hide the buttons and mark ourselves (including children) as no
  // longer processing events.
  SetCanProcessEventsWithinSubtree(false);

  desk_action_view_->SetVisible(false);

  controller->RemoveDesk(desk_, DesksCreationRemovalSource::kButton,
                         close_type);
}

const char* DeskMiniView::GetClassName() const {
  return "DeskMiniView";
}

void DeskMiniView::Layout() {
  const gfx::Rect preview_bounds = GetDeskPreviewBounds(root_window_);
  desk_preview_->SetBoundsRect(preview_bounds);

  LayoutDeskNameView(preview_bounds);
  const gfx::Size desk_action_view_size = desk_action_view_->GetPreferredSize();
  desk_action_view_->SetBounds(
      preview_bounds.right() - desk_action_view_size.width() -
          kCloseButtonMargin,
      kCloseButtonMargin, desk_action_view_size.width(),
      desk_action_view_size.height());
}

gfx::Size DeskMiniView::CalculatePreferredSize() const {
  const gfx::Rect preview_bounds = GetDeskPreviewBounds(root_window_);

  // The preferred size takes into account only the width of the preview
  // view. Desk preview's bottom inset should be excluded to maintain
  // |kLabelPreviewSpacing| between preview and desk name view.
  return gfx::Size{preview_bounds.width(),
                   preview_bounds.height() - GetPreviewBorderInsets().bottom() +
                       2 * kLabelPreviewSpacing +
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

  // If the desk can be combined or closed, add a tip to let the user know they
  // can use an accelerator.
  if (!DesksController::Get()->CanRemoveDesks())
    return;

  const std::u16string target_desk_name =
      DesksController::Get()->GetCombineDesksTargetName(desk_);
  const std::string extra_tip = l10n_util::GetStringFUTF8(
      IDS_ASH_OVERVIEW_CLOSABLE_DESK_MINIVIEW_A11Y_EXTRA_TIP, target_desk_name);

  node_data->AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                extra_tip);
}

void DeskMiniView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateFocusColor();
}

void DeskMiniView::OnContentChanged() {
  DCHECK(desk_preview_);
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

void DeskMiniView::OnDeskNameChanged(const std::u16string& new_name) {
  if (is_desk_name_being_modified_)
    return;

  desk_name_view_->SetText(new_name);
  desk_name_view_->SetAccessibleName(new_name);
  desk_preview_->SetAccessibleName(new_name);

  Layout();
}

void DeskMiniView::ContentsChanged(views::Textfield* sender,
                                   const std::u16string& new_contents) {
  DCHECK_EQ(sender, desk_name_view_);
  DCHECK(is_desk_name_being_modified_);
  if (!desk_)
    return;

  // To avoid potential security and memory issues, we don't allow desk names to
  // have an unbounded length. Therefore we trim if needed at kMaxLength UTF-16
  // boundary. Note that we don't care about code point boundaries in this case.
  if (new_contents.size() > DesksTextfield::kMaxLength) {
    std::u16string trimmed_new_contents = new_contents;
    trimmed_new_contents.resize(DesksTextfield::kMaxLength);
    desk_name_view_->SetText(trimmed_new_contents);
  }

  Layout();
}

bool DeskMiniView::HandleKeyEvent(views::Textfield* sender,
                                  const ui::KeyEvent& key_event) {
  DCHECK_EQ(sender, desk_name_view_);
  DCHECK(is_desk_name_being_modified_);

  // Pressing enter or escape should blur the focus away from DeskNameView so
  // that editing the desk's name ends. Pressing tab should do the same, but is
  // handled in OverviewSession.
  if (key_event.type() != ui::ET_KEY_PRESSED)
    return false;

  if (key_event.key_code() != ui::VKEY_RETURN &&
      key_event.key_code() != ui::VKEY_ESCAPE) {
    return false;
  }

  // If the escape key was pressed, `should_commit_name_changes_` is set to
  // false so that OnViewBlurred knows that it should not change the name of
  // `desk_`.
  if (key_event.key_code() == ui::VKEY_ESCAPE)
    should_commit_name_changes_ = false;

  DeskNameView::CommitChanges(GetWidget());

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

  // Assume we should commit the name change unless HandleKeyEvent detects the
  // user pressed the escape key.
  should_commit_name_changes_ = true;

  // Set the Overview highlight to move focus with the DeskNameView.
  UpdateOverviewHighlightForFocus(desk_name_view_);

  if (!defer_select_all_)
    desk_name_view_->SelectAll(false);
}

void DeskMiniView::OnViewBlurred(views::View* observed_view) {
  DCHECK_EQ(observed_view, desk_name_view_);
  defer_select_all_ = false;

  // If `should_commit_name_changes_` is true, then the view was blurred from
  // the user pressing a key other than escape. In that case we should set the
  // name of `desk_` to the new name contained in `desk_name_view_`. Order here
  // matters, because Desk::SetName calls OnDeskNameChanged on each of its
  // observers, and DeskMiniView::OnDeskNameChanged will only set the text in
  // `desk_name_view_` to the name contained in `desk_` when
  // `is_desk_name_being_modified_` is set to false. So, to avoid performing the
  // operations in OnDeskNameChanged twice, we need to call SetName before
  // setting `is_desk_name_being_modified_` to false.
  if (should_commit_name_changes_) {
    desk_->SetName(
        base::CollapseWhitespace(desk_name_view_->GetText(),
                                 /*trim_sequences_with_line_breaks=*/false),
        /*set_by_user=*/true);
  }

  // When committing the name, do not allow an empty desk name. Revert back to
  // the default name if the desk is not being removed.
  // TODO(afakhry): Make this more robust. What if user renames a previously
  // user-modified desk name, say from "code" to "Desk 2", and that desk
  // happened to be in the second position. Since the new name matches the
  // default one for this position, should we revert it (i.e. consider it
  // `set_by_user = false`?
  if (!desk_->is_desk_being_removed() && desk_->name().empty()) {
    // DeskController::RevertDeskNameToDefault calls Desk::SetName, so we should
    // call this before we set `is_desk_name_being_modified_` to false for the
    // same reason that we call Desk::SetName before.
    DesksController::Get()->RevertDeskNameToDefault(desk_);
  }

  is_desk_name_being_modified_ = false;
  OnDeskNameChanged(desk_->name());

  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_DESKS_DESK_NAME_COMMIT, desk_->name()));

  // Only when the new desk name has been committed is when we can update the
  // desks restore prefs.
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();
}

void DeskMiniView::OnContextMenuClosed() {
  is_context_menu_open_ = false;
  UpdateDeskButtonVisibility();
  desk_preview_->SetHighlightOverlayVisibility(false);
}

void DeskMiniView::OnDeskPreviewPressed() {
  // If there is an ongoing desk activation, do nothing.
  DesksController* desks_controller = DesksController::Get();
  if (!desks_controller->AreDesksBeingModified())
    desks_controller->ActivateDesk(desk_, DesksSwitchSource::kMiniViewButton);
}

void DeskMiniView::LayoutDeskNameView(const gfx::Rect& preview_bounds) {
  const gfx::Size desk_name_view_size = desk_name_view_->GetPreferredSize();
  // Desk preview's width is supposed to be larger than kMinDeskNameViewWidth,
  // but it might be not the truth for tests with extreme abnormal size of
  // display. The preview uses a border to display focus and the name view uses
  // a focus ring (which does not inset the view), so subtract the focus ring
  // from the size calculations so that the focus UI is aligned.
  views::FocusRing* focus_ring = views::FocusRing::Get(desk_name_view_);
  const int focus_ring_length =
      focus_ring->GetHaloThickness() - focus_ring->GetHaloInset();
  const int min_width = std::min(preview_bounds.width() - focus_ring_length,
                                 kMinDeskNameViewWidth);
  const int max_width = std::max(preview_bounds.width() - focus_ring_length,
                                 kMinDeskNameViewWidth);
  const int text_width =
      base::clamp(desk_name_view_size.width(), min_width, max_width);
  const int desk_name_view_x =
      preview_bounds.x() + (preview_bounds.width() - text_width) / 2;
  gfx::Rect desk_name_view_bounds{desk_name_view_x,
                                  preview_bounds.bottom() -
                                      GetPreviewBorderInsets().bottom() +
                                      kLabelPreviewSpacing,
                                  text_width, desk_name_view_size.height()};
  desk_name_view_->SetBoundsRect(desk_name_view_bounds);
}

}  // namespace ash
