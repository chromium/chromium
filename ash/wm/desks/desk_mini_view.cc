// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_mini_view.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/close_button.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_action_context_menu.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/desks_textfield.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "base/bind.h"
#include "base/cxx17_backports.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

constexpr int kLabelPreviewSpacing = 8;

constexpr int kCloseButtonMargin = 8;

constexpr int kMinDeskNameViewWidth = 56;

gfx::Rect ConvertScreenRect(views::View* view, const gfx::Rect& screen_rect) {
  gfx::Point origin = screen_rect.origin();
  views::View::ConvertPointFromScreen(view, &origin);
  return gfx::Rect(origin, screen_rect.size());
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

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // TODO(afakhry): Tooltips.

  desk_preview_ = AddChildView(std::make_unique<DeskPreviewView>(
      base::BindRepeating(&DeskMiniView::OnDeskPreviewPressed,
                          base::Unretained(this)),
      this));
  desk_name_view_ = AddChildView(std::move(desk_name_view));

  if (features::IsDesksCloseAllEnabled()) {
    // TODO(crbug.com/1308429): Replace PLACEHOLDER with the name of the initial
    // target desk for combine desks operation.
    std::u16string initial_combine_desks_target_name = u"PLACEHOLDER";

    desk_action_view_ = AddChildView(std::make_unique<DeskActionView>(
        initial_combine_desks_target_name,
        /*combine_desks_callback=*/
        base::BindRepeating(&DeskMiniView::OnRemovingDesk,
                            base::Unretained(this), /*close_windows=*/false),
        /*close_all_callback=*/
        base::BindRepeating(&DeskMiniView::OnRemovingDesk,
                            base::Unretained(this), /*close_windows=*/true)));

    context_menu_ = std::make_unique<DeskActionContextMenu>(
        initial_combine_desks_target_name,
        /*combine_desks_callback=*/
        base::BindRepeating(&DeskMiniView::OnRemovingDesk,
                            base::Unretained(this), /*close_windows=*/false),
        /*close_all_callback=*/
        base::BindRepeating(&DeskMiniView::OnRemovingDesk,
                            base::Unretained(this), /*close_windows=*/true),
        base::BindRepeating(&DeskMiniView::OnContextMenuClosed,
                            base::Unretained(this)));

    // TODO(crbug.com/1308429): Can initialize highlight overlay here.
  } else {
    close_desk_button_ = AddChildView(std::make_unique<CloseButton>(
        base::BindRepeating(&DeskMiniView::OnRemovingDesk,
                            base::Unretained(this), /*close_windows=*/false),
        CloseButton::Type::kSmall));
  }
  UpdateDeskButtonVisibility();
  UpdateBorderColor();
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
  // Don't show desk buttons when hovered while the dragged window is on
  // the DesksBarView.
  // For switch access, setting desk buttons to visible allows users to
  // navigate to it.
  const bool visible =
      DesksController::Get()->CanRemoveDesks() &&
      !owner_bar_->dragged_item_over_bar() && !owner_bar_->IsDraggingDesk() &&
      (IsMouseHovered() || force_show_desk_buttons_ ||
       Shell::Get()->accessibility_controller()->IsSwitchAccessRunning());

  if (features::IsDesksCloseAllEnabled())
    desk_action_view_->SetVisible(visible && !is_context_menu_open_);
  else
    close_desk_button_->SetVisible(visible);
}

void DeskMiniView::OnWidgetGestureTap(const gfx::Rect& screen_rect,
                                      bool is_long_gesture) {
  views::View* view_to_update =
      features::IsDesksCloseAllEnabled()
          ? static_cast<views::View*>(desk_action_view_)
          : static_cast<views::View*>(close_desk_button_);

  // Note that we don't want to hide the desk buttons if it's a single tap
  // within the bounds of an already visible button, which will later be
  // handled as a press event on that desk buttons that will result in closing
  // the desk.
  const bool old_force_show_desk_buttons = force_show_desk_buttons_;
  force_show_desk_buttons_ =
      (is_long_gesture && IsPointOnMiniView(screen_rect.CenterPoint())) ||
      (!is_long_gesture && view_to_update->GetVisible() &&
       view_to_update->HitTestRect(
           ConvertScreenRect(view_to_update, screen_rect)));

  if (old_force_show_desk_buttons != force_show_desk_buttons_)
    UpdateDeskButtonVisibility();
}

void DeskMiniView::UpdateBorderColor() {
  DCHECK(desk_);
  auto* color_provider = AshColorProvider::Get();
  if ((owner_bar_->dragged_item_over_bar() &&
       IsPointOnMiniView(owner_bar_->last_dragged_item_screen_location())) ||
      IsViewHighlighted()) {
    desk_preview_->SetBorderColor(color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kFocusRingColor));
  } else if (!desk_->is_active() ||
             owner_bar_->overview_grid()->IsShowingDesksTemplatesGrid()) {
    desk_preview_->SetBorderColor(SK_ColorTRANSPARENT);
  } else {
    desk_preview_->SetBorderColor(color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kCurrentDeskColor));
  }
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

void DeskMiniView::OpenContextMenu() {
  is_context_menu_open_ = true;
  UpdateDeskButtonVisibility();

  // TODO(crbug.com/1308429): Should set highlight overlay to visible and update
  // context menu item label for combining desks here to tell the user where the
  // windows will go.

  // TODO(crbug.com/1308780): Source will need to be different when opening with
  // long press and possibly keyboard.
  context_menu_->ShowContextMenuForView(
      this, desk_preview_->GetBoundsInScreen().bottom_left(),
      ui::MENU_SOURCE_MOUSE);
}

const char* DeskMiniView::GetClassName() const {
  return "DeskMiniView";
}

void DeskMiniView::Layout() {
  const gfx::Rect preview_bounds = GetDeskPreviewBounds(root_window_);
  desk_preview_->SetBoundsRect(preview_bounds);

  LayoutDeskNameView(preview_bounds);
  if (features::IsDesksCloseAllEnabled()) {
    const gfx::Size desk_action_view_size =
        desk_action_view_->GetPreferredSize();
    desk_action_view_->SetBounds(
        preview_bounds.right() - desk_action_view_size.width() -
            kCloseButtonMargin,
        kCloseButtonMargin, desk_action_view_size.width(),
        desk_action_view_size.height());

    // TODO(crbug.com/1308429): Set bounds for a highlight overlay.
  } else {
    DCHECK(close_desk_button_);
    const int close_button_size =
        close_desk_button_->GetPreferredSize().width();
    close_desk_button_->SetBounds(
        preview_bounds.right() - close_button_size - kCloseButtonMargin,
        kCloseButtonMargin, close_button_size, close_button_size);
  }
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

  if (DesksController::Get()->CanRemoveDesks()) {
    node_data->AddStringAttribute(
        ax::mojom::StringAttribute::kDescription,
        l10n_util::GetStringUTF8(
            IDS_ASH_OVERVIEW_CLOSABLE_HIGHLIGHT_ITEM_A11Y_EXTRA_TIP));
  }
}

void DeskMiniView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateBorderColor();
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

views::View* DeskMiniView::GetView() {
  return this;
}

void DeskMiniView::MaybeActivateHighlightedView() {
  DesksController::Get()->ActivateDesk(desk(),
                                       DesksSwitchSource::kMiniViewButton);
}

void DeskMiniView::MaybeCloseHighlightedView() {
  // TODO(crbug.com/1307011): This function is called when we press ctrl+W
  // while highlighted over a desk mini view to combine desks. Should be
  // reworked when we add an accelerator for close-all.
  OnRemovingDesk(/*close_windows=*/false);
}

void DeskMiniView::MaybeSwapHighlightedView(bool right) {
  const int old_index = owner_bar_->GetMiniViewIndex(this);
  DCHECK_NE(old_index, -1);

  const bool mirrored = owner_bar_->GetMirrored();
  // If mirrored, flip the swap direction.
  int new_index = mirrored ^ right ? old_index + 1 : old_index - 1;
  if (new_index < 0 ||
      new_index == static_cast<int>(owner_bar_->mini_views().size())) {
    return;
  }

  auto* desks_controller = DesksController::Get();
  desks_controller->ReorderDesk(old_index, new_index);
  desks_controller->UpdateDesksDefaultNames();
}

bool DeskMiniView::MaybeActivateHighlightedViewOnOverviewExit(
    OverviewSession* overview_session) {
  MaybeActivateHighlightedView();
  return true;
}

void DeskMiniView::OnViewHighlighted() {
  UpdateBorderColor();
  owner_bar_->ScrollToShowMiniViewIfNecessary(this);
}

void DeskMiniView::OnViewUnhighlighted() {
  UpdateBorderColor();
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
  desk_name_view_->UpdateViewAppearance();

  // Set the Overview highlight to move focus with the DeskNameView.
  auto* highlight_controller = Shell::Get()
                                   ->overview_controller()
                                   ->overview_session()
                                   ->highlight_controller();
  if (highlight_controller->IsFocusHighlightVisible())
    highlight_controller->MoveHighlightToView(desk_name_view_);

  if (!defer_select_all_)
    desk_name_view_->SelectAll(false);
}

void DeskMiniView::OnViewBlurred(views::View* observed_view) {
  DCHECK_EQ(observed_view, desk_name_view_);
  defer_select_all_ = false;
  desk_name_view_->UpdateViewAppearance();

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

void DeskMiniView::OnRemovingDesk(bool close_windows) {
  if (!desk_)
    return;

  auto* controller = DesksController::Get();
  if (!controller->CanRemoveDesks())
    return;

  // We want to avoid the possibility of getting triggered multiple times. We
  // therefore hide the buttons and mark ourselves (including children) as no
  // longer processing events.
  SetCanProcessEventsWithinSubtree(false);

  if (features::IsDesksCloseAllEnabled())
    desk_action_view_->SetVisible(false);
  else
    close_desk_button_->SetVisible(false);

  desk_preview_->OnRemovingDesk();

  controller->RemoveDesk(desk_, DesksCreationRemovalSource::kButton,
                         close_windows);
}

void DeskMiniView::OnContextMenuClosed() {
  is_context_menu_open_ = false;
  UpdateDeskButtonVisibility();
  // TODO(crbug.com/1308429): Make highlight overlay visibility false here.
}

void DeskMiniView::OnDeskPreviewPressed() {
  DesksController::Get()->ActivateDesk(desk_,
                                       DesksSwitchSource::kMiniViewButton);
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
      focus_ring->halo_thickness() - focus_ring->halo_inset();
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
