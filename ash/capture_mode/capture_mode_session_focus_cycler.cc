// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_session_focus_cycler.h"

#include <vector>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/magnifier/magnifier_utils.h"
#include "ash/accessibility/scoped_a11y_override_window_setter.h"
#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_button.h"
#include "ash/capture_mode/capture_mode_camera_preview_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/style_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The focusable items for the FocusGroup::kSelection group.
constexpr std::array<FineTunePosition, 9> kSelectionTabbingOrder = {
    FineTunePosition::kCenter,       FineTunePosition::kTopLeft,
    FineTunePosition::kTopCenter,    FineTunePosition::kTopRight,
    FineTunePosition::kRightCenter,  FineTunePosition::kBottomRight,
    FineTunePosition::kBottomCenter, FineTunePosition::kBottomLeft,
    FineTunePosition::kLeftCenter};

std::vector<aura::Window*> GetWindowListIgnoreModalForActiveDesk() {
  return Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
      DesksMruType::kActiveDesk);
}

views::Widget* GetCameraPreviewWidget() {
  auto* camera_controller = CaptureModeController::Get()->camera_controller();
  return camera_controller ? camera_controller->camera_preview_widget()
                           : nullptr;
}

CameraPreviewView* GetCameraPreviewView() {
  auto* camera_controller = CaptureModeController::Get()->camera_controller();
  return camera_controller ? camera_controller->camera_preview_view() : nullptr;
}

}  // namespace

// -----------------------------------------------------------------------------
// CaptureModeSessionFocusCycler::HighlightableView:

std::unique_ptr<views::HighlightPathGenerator>
CaptureModeSessionFocusCycler::HighlightableView::CreatePathGenerator() {
  return nullptr;
}

void CaptureModeSessionFocusCycler::HighlightableView::PseudoFocus() {
  has_focus_ = true;

  views::View* view = GetView();
  DCHECK(view);

  // This is lazy initialization of the FocusRing effectively. This is only used
  // for children of HighlightableView, so it will not replace any other style
  // of FocusRing.
  if (!focus_ring_) {
    focus_ring_ = StyleUtil::SetUpFocusRingForView(view);
    // Use a custom focus predicate as the default one checks if |view| actually
    // has focus which won't be happening since our widgets are not activatable.
    focus_ring_->SetHasFocusPredicate(
        [&](views::View* view) { return view->GetVisible() && has_focus_; });

    auto path_generator = CreatePathGenerator();
    if (path_generator)
      focus_ring_->SetPathGenerator(std::move(path_generator));
  }

  focus_ring_->Layout();
  focus_ring_->SchedulePaint();

  view->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);

  magnifier_utils::MaybeUpdateActiveMagnifierFocus(
      view->GetBoundsInScreen().CenterPoint());
}

void CaptureModeSessionFocusCycler::HighlightableView::PseudoBlur() {
  has_focus_ = false;

  if (!focus_ring_)
    return;

  focus_ring_->Layout();
  focus_ring_->SchedulePaint();
}

void CaptureModeSessionFocusCycler::HighlightableView::ClickView() {
  views::View* view = GetView();
  DCHECK(view);

  views::Button* button = views::Button::AsButton(view);
  if (!button)
    return;
  button->AcceleratorPressed(ui::Accelerator(ui::VKEY_SPACE, /*modifiers=*/0));
}

// -----------------------------------------------------------------------------
// CaptureModeSessionFocusCycler:

CaptureModeSessionFocusCycler::CaptureModeSessionFocusCycler(
    CaptureModeSession* session)
    : groups_for_fullscreen_{FocusGroup::kNone, FocusGroup::kTypeSource,
                             FocusGroup::kCameraPreview,
                             FocusGroup::kSettingsMenu,
                             FocusGroup::kSettingsClose},
      groups_for_region_{FocusGroup::kNone,          FocusGroup::kTypeSource,
                         FocusGroup::kSelection,     FocusGroup::kCameraPreview,
                         FocusGroup::kCaptureButton, FocusGroup::kSettingsMenu,
                         FocusGroup::kSettingsClose},
      groups_for_window_{FocusGroup::kNone, FocusGroup::kTypeSource,
                         FocusGroup::kCaptureWindow, FocusGroup::kSettingsMenu,
                         FocusGroup::kSettingsClose},
      session_(session),
      scoped_a11y_overrider_(
          std::make_unique<ScopedA11yOverrideWindowSetter>()) {}

CaptureModeSessionFocusCycler::~CaptureModeSessionFocusCycler() = default;

void CaptureModeSessionFocusCycler::AdvanceFocus(bool reverse) {
  // Advancing focus while the settings menu is open will close the menu and
  // clear focus, unless the settings menu was opened using keyboard navigation.
  if (!settings_menu_opened_with_keyboard_nav_) {
    views::Widget* settings_widget = GetSettingsMenuWidget();
    if (settings_widget && settings_widget->IsVisible()) {
      session_->SetSettingsMenuShown(false);
      return;
    }
  }

  ClearCurrentVisibleFocus();

  FocusGroup previous_focus_group = current_focus_group_;
  const size_t previous_group_size = GetGroupSize(previous_focus_group);
  const size_t previous_focus_index = focus_index_;

  // Go to the next group if the next index is out of bounds for the current
  // group. Otherwise, update |focus_index_| depending on |reverse|.
  if (!reverse && (previous_group_size == 0u ||
                   previous_focus_index >= previous_group_size - 1u)) {
    current_focus_group_ = GetNextGroup(/*reverse=*/false);
    focus_index_ = 0u;
  } else if (reverse && previous_focus_index == 0u) {
    current_focus_group_ = GetNextGroup(/*reverse=*/true);
    // The size of FocusGroup::kCaptureWindow could be empty.
    focus_index_ = std::max(
        static_cast<int32_t>(GetGroupSize(current_focus_group_)) - 1, 0);
  } else {
    focus_index_ = reverse ? focus_index_ - 1u : focus_index_ + 1u;
  }
  scoped_a11y_overrider_->MaybeUpdateA11yOverrideWindow(
      GetA11yOverrideWindow());

  if (current_focus_group_ == FocusGroup::kCaptureWindow) {
    AdvanceFocusInsideCaptureWindow(focus_index_, reverse);
    return;
  }

  auto* camera_preview_view = GetCameraPreviewView();
  if (!reverse && camera_preview_view &&
      previous_focus_group == FocusGroup::kCaptureWindow) {
    camera_preview_view->PseudoBlur();
  }

  // Focus the new item.
  std::vector<HighlightableView*> current_views =
      GetGroupItems(current_focus_group_);
  if (!current_views.empty()) {
    DCHECK_LT(focus_index_, current_views.size());
    current_views[focus_index_]->PseudoFocus();
  }

  // Selection focus is drawn directly on a layer owned by |session_|. Notify
  // the layer to repaint if necessary.
  const bool current_group_is_selection =
      current_focus_group_ == FocusGroup::kSelection;
  const bool redraw_layer = previous_focus_group == FocusGroup::kSelection ||
                            current_group_is_selection;

  if (redraw_layer)
    session_->RepaintRegion();

  if (current_group_is_selection) {
    const gfx::Rect user_region =
        CaptureModeController::Get()->user_capture_region();
    if (user_region.IsEmpty())
      return;

    const auto fine_tune_position = GetFocusedFineTunePosition();
    DCHECK_NE(fine_tune_position, FineTunePosition::kNone);

    gfx::Point point_of_interest =
        fine_tune_position == FineTunePosition::kCenter
            ? user_region.CenterPoint()
            : capture_mode_util::GetLocationForFineTunePosition(
                  user_region, fine_tune_position);
    wm::ConvertPointToScreen(session_->current_root(), &point_of_interest);
    magnifier_utils::MaybeUpdateActiveMagnifierFocus(point_of_interest);

    return;
  }
}

void CaptureModeSessionFocusCycler::ClearFocus() {
  ClearCurrentVisibleFocus();

  if (current_focus_group_ == FocusGroup::kSelection)
    session_->RepaintRegion();

  current_focus_group_ = FocusGroup::kNone;
  focus_index_ = 0u;
}

bool CaptureModeSessionFocusCycler::HasFocus() const {
  return current_focus_group_ != FocusGroup::kNone;
}

bool CaptureModeSessionFocusCycler::OnSpacePressed() {
  if (current_focus_group_ == FocusGroup::kNone ||
      current_focus_group_ == FocusGroup::kSelection ||
      current_focus_group_ == FocusGroup::kPendingSettings ||
      current_focus_group_ == FocusGroup::kCaptureWindow ||
      current_focus_group_ == FocusGroup::kCameraPreview) {
    return false;
  }

  std::vector<HighlightableView*> views = GetGroupItems(current_focus_group_);
  if (views.empty())
    return false;

  // If current focused view doesn't exist, return directly.
  if (!FindFocusedViewAndUpdateFocusIndex(views))
    return false;

  DCHECK(!views.empty());
  DCHECK_LT(focus_index_, views.size());
  HighlightableView* view = views[focus_index_];

  // Let the session handle the space key event if the region toggle button
  // currently has focus and we are already in region mode, as we still want to
  // create a default region in this case.
  CaptureModeBarView* bar_view = session_->capture_mode_bar_view_;
  if (view->GetView() ==
          bar_view->capture_source_view()->region_toggle_button() &&
      CaptureModeController::Get()->source() == CaptureModeSource::kRegion) {
    return false;
  }

  // Clicking on the settings button first clears current focus and moves us to
  // a temporary state. The next focus signal will navigate through the settings
  // items.
  if (view->GetView() == session_->capture_mode_bar_view_->settings_button()) {
    settings_menu_opened_with_keyboard_nav_ = true;
    ClearCurrentVisibleFocus();
    current_focus_group_ = FocusGroup::kPendingSettings;
    focus_index_ = 0u;
  }

  // ClickView comes last as it will destroy |this| if |view| is the close
  // button.
  view->ClickView();
  return true;
}

bool CaptureModeSessionFocusCycler::RegionGroupFocused() const {
  return current_focus_group_ == FocusGroup::kSelection ||
         current_focus_group_ == FocusGroup::kCaptureButton;
}

bool CaptureModeSessionFocusCycler::CaptureBarFocused() const {
  return current_focus_group_ == FocusGroup::kTypeSource ||
         current_focus_group_ == FocusGroup::kSettingsClose ||
         current_focus_group_ == FocusGroup::kPendingSettings;
}

bool CaptureModeSessionFocusCycler::CaptureLabelFocused() const {
  return current_focus_group_ == FocusGroup::kCaptureButton;
}

FineTunePosition CaptureModeSessionFocusCycler::GetFocusedFineTunePosition()
    const {
  if (current_focus_group_ != FocusGroup::kSelection)
    return FineTunePosition::kNone;
  return kSelectionTabbingOrder[focus_index_];
}

void CaptureModeSessionFocusCycler::OnCaptureLabelWidgetUpdated() {
  UpdateA11yAnnotation();
}

void CaptureModeSessionFocusCycler::OnSettingsMenuWidgetCreated() {
  views::Widget* settings_menu_widget =
      session_->capture_mode_settings_widget_.get();
  DCHECK(settings_menu_widget);
  settings_menu_widget_observeration_.Observe(settings_menu_widget);
  UpdateA11yAnnotation();
}

void CaptureModeSessionFocusCycler::OnWidgetClosing(views::Widget* widget) {
  settings_menu_opened_with_keyboard_nav_ = false;
  settings_menu_widget_observeration_.Reset();

  // Return immediately if the widget is closing by the closing of `session_`.
  if (session_->is_shutting_down())
    return;
  // Remove focus if one of the settings related groups is currently
  // focused.
  if (current_focus_group_ == FocusGroup::kPendingSettings ||
      current_focus_group_ == FocusGroup::kSettingsMenu) {
    ClearFocus();
  }
  UpdateA11yAnnotation();
}

void CaptureModeSessionFocusCycler::ClearCurrentVisibleFocus() {
  // The settings menu widget may be destroyed while it has focus. No need to
  // clear focus in this case.
  if (current_focus_group_ == FocusGroup::kSettingsMenu &&
      !GetSettingsMenuWidget()) {
    return;
  }

  std::vector<HighlightableView*> views = GetGroupItems(current_focus_group_);
  if (views.empty())
    return;

  // If current focused view doesn't exist, return directly.
  if (!FindFocusedViewAndUpdateFocusIndex(views))
    return;

  DCHECK_LT(focus_index_, views.size());
  views[focus_index_]->PseudoBlur();
}

CaptureModeSessionFocusCycler::FocusGroup
CaptureModeSessionFocusCycler::GetNextGroup(bool reverse) const {
  if (current_focus_group_ == FocusGroup::kPendingSettings) {
    DCHECK(GetSettingsMenuWidget());
    return FocusGroup::kSettingsMenu;
  }

  const std::vector<FocusGroup>& groups_list = GetCurrentGroupList();
  const int increment = reverse ? -1 : 1;
  const auto iter = base::ranges::find(groups_list, current_focus_group_);
  DCHECK(iter != groups_list.end());
  size_t next_group_index = std::distance(groups_list.begin(), iter);

  do {
    next_group_index = (next_group_index + increment) % groups_list.size();
  } while (!IsGroupAvailable(groups_list[next_group_index]));

  return groups_list[next_group_index];
}

const std::vector<CaptureModeSessionFocusCycler::FocusGroup>&
CaptureModeSessionFocusCycler::GetCurrentGroupList() const {
  switch (session_->controller_->source()) {
    case CaptureModeSource::kFullscreen:
      return groups_for_fullscreen_;
    case CaptureModeSource::kRegion:
      return groups_for_region_;
    case CaptureModeSource::kWindow:
      return groups_for_window_;
  }
}

bool CaptureModeSessionFocusCycler::IsGroupAvailable(FocusGroup group) const {
  switch (group) {
    case FocusGroup::kNone:
    case FocusGroup::kTypeSource:
    case FocusGroup::kSettingsClose:
    case FocusGroup::kPendingSettings:
      return true;
    case FocusGroup::kSelection:
    case FocusGroup::kCaptureButton: {
      // The selection UI and capture button are focusable only when the label
      // button of CaptureLabelView is visible.
      auto* widget = session_->capture_label_widget_.get();
      if (!widget)
        return false;
      auto* capture_label_view =
          static_cast<CaptureLabelView*>(widget->GetContentsView());
      return capture_label_view->label_button()->GetVisible();
    }
    case FocusGroup::kCaptureWindow:
      return session_->controller_->source() == CaptureModeSource::kWindow &&
             GetGroupSize(FocusGroup::kCaptureWindow) > 0;
    case FocusGroup::kSettingsMenu:
      return session_->capture_mode_settings_view_;
    case FocusGroup::kCameraPreview: {
      auto* camera_preview_widget = GetCameraPreviewWidget();
      return camera_preview_widget && camera_preview_widget->IsVisible();
    }
  }
}

size_t CaptureModeSessionFocusCycler::GetGroupSize(FocusGroup group) const {
  if (group == FocusGroup::kSelection)
    return 9u;
  if (group == FocusGroup::kCaptureWindow) {
    const size_t windows_size = GetWindowListIgnoreModalForActiveDesk().size();
    if (!GetCameraPreviewWidget())
      return windows_size;
    // TODO(crbug/1306563): Only count the windows that large enough to have the
    // camera preview inside.
    return windows_size *
           (1 + GetGroupItems(FocusGroup::kCameraPreview).size());
  }
  return GetGroupItems(group).size();
}

std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
CaptureModeSessionFocusCycler::GetGroupItems(FocusGroup group) const {
  std::vector<HighlightableView*> items;
  switch (group) {
    case FocusGroup::kNone:
    case FocusGroup::kSelection:
    case FocusGroup::kPendingSettings:
    case FocusGroup::kCaptureWindow:
      break;
    case FocusGroup::kTypeSource: {
      CaptureModeBarView* bar_view = session_->capture_mode_bar_view_;
      CaptureModeTypeView* type_view = bar_view->capture_type_view();
      CaptureModeSourceView* source_view = bar_view->capture_source_view();
      items = {type_view->image_toggle_button(),
               type_view->video_toggle_button(),
               source_view->fullscreen_toggle_button(),
               source_view->region_toggle_button(),
               source_view->window_toggle_button()};

      base::EraseIf(items,
                    [](CaptureModeSessionFocusCycler::HighlightableView* item) {
                      return !item || !item->GetView()->GetEnabled();
                    });
      break;
    }
    case FocusGroup::kCaptureButton: {
      views::Widget* widget = session_->capture_label_widget_.get();
      DCHECK(widget);
      items = {static_cast<CaptureLabelView*>(widget->GetContentsView())};
      break;
    }
    case FocusGroup::kSettingsClose: {
      CaptureModeBarView* bar_view = session_->capture_mode_bar_view_;
      items = {bar_view->settings_button(), bar_view->close_button()};
      break;
    }
    case FocusGroup::kSettingsMenu: {
      CaptureModeSettingsView* settings_view =
          session_->capture_mode_settings_view_;
      DCHECK(settings_view);
      items = settings_view->GetHighlightableItems();
      break;
    }
    case FocusGroup::kCameraPreview: {
      auto* camera_preview_view = GetCameraPreviewView();
      DCHECK(camera_preview_view);
      items = {camera_preview_view};
      break;
    }
  }
  return items;
}

views::Widget* CaptureModeSessionFocusCycler::GetSettingsMenuWidget() const {
  return session_->capture_mode_settings_widget_.get();
}

aura::Window* CaptureModeSessionFocusCycler::GetA11yOverrideWindow() const {
  switch (current_focus_group_) {
    case FocusGroup::kCaptureButton:
      return session_->capture_label_widget()->GetNativeWindow();
    case FocusGroup::kSettingsMenu:
      return session_->capture_mode_settings_widget()->GetNativeWindow();
    case FocusGroup::kNone:
    case FocusGroup::kTypeSource:
    case FocusGroup::kSelection:
    case FocusGroup::kCaptureWindow:
    case FocusGroup::kSettingsClose:
    case FocusGroup::kPendingSettings:
      return session_->capture_mode_bar_widget()->GetNativeWindow();
    case FocusGroup::kCameraPreview:
      return GetCameraPreviewWidget()->GetNativeWindow();
  }
}

bool CaptureModeSessionFocusCycler::FindFocusedViewAndUpdateFocusIndex(
    std::vector<HighlightableView*> views) {
  const size_t current_focus_index =
      std::find_if(views.begin(), views.end(),
                   [](CaptureModeSessionFocusCycler::HighlightableView* item) {
                     return item->has_focus();
                   }) -
      views.begin();

  // If current focused view doesn't exist, return false;
  if (current_focus_index == views.size()) {
    // If `focus_index_` is out of bound, update it to the last index of the
    // `views`.
    if (focus_index_ >= views.size())
      focus_index_ = views.size() - 1;
    return false;
  }

  // Update `focus_index_` to ensure it's up to date, since highlightable views
  // of `current_focus_group_` can be updated during keyboard navigation, for
  // example, the custom folder option can be added or removed via the select
  // folder menu item.
  focus_index_ = current_focus_index;
  return true;
}

void CaptureModeSessionFocusCycler::UpdateA11yAnnotation() {
  std::vector<views::Widget*> a11y_widgets;

  // If the bar widget is not available, then this is called while shutting
  // down the capture mode session.
  views::Widget* bar_widget = session_->capture_mode_bar_widget_.get();
  if (bar_widget)
    a11y_widgets.push_back(bar_widget);

  // Add the label widget only if the button is visible.
  views::Widget* label_widget = session_->capture_label_widget_.get();
  if (label_widget &&
      static_cast<CaptureLabelView*>(label_widget->GetContentsView())
          ->label_button()
          ->GetVisible()) {
    a11y_widgets.push_back(label_widget);
  }

  // Add the settings widget if it exists.
  views::Widget* settings_menu_widget =
      session_->capture_mode_settings_widget_.get();
  if (settings_menu_widget)
    a11y_widgets.push_back(settings_menu_widget);

  // Helper to update |target|'s a11y focus with |previous| and |next|, which
  // can be null.
  auto update_a11y_widget_focus =
      [](views::Widget* target, views::Widget* previous, views::Widget* next) {
        DCHECK(target);
        auto* contents_view = target->GetContentsView();
        auto& view_a11y = contents_view->GetViewAccessibility();
        view_a11y.OverridePreviousFocus(previous);
        view_a11y.OverrideNextFocus(next);
        contents_view->NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged,
                                                true);
      };

  // If there is only one widget left, clear the focus overrides so that they
  // do not point to deleted objects.
  if (a11y_widgets.size() == 1u) {
    update_a11y_widget_focus(a11y_widgets[0], nullptr, nullptr);
    return;
  }

  const int size = a11y_widgets.size();
  for (int i = 0; i < size; ++i) {
    const int previous_index = (i + size - 1) % size;
    const int next_index = (i + 1) % size;
    update_a11y_widget_focus(a11y_widgets[i], a11y_widgets[previous_index],
                             a11y_widgets[next_index]);
  }
}

void CaptureModeSessionFocusCycler::AdvanceFocusInsideCaptureWindow(
    size_t focus_index,
    bool reverse) {
  auto* camera_preview_view = GetCameraPreviewView();
  const std::vector<aura::Window*> windows =
      GetWindowListIgnoreModalForActiveDesk();
  const size_t group_items_size = GetGroupSize(FocusGroup::kCaptureWindow);
  if (windows.empty() || focus_index >= group_items_size) {
    AdvanceFocus(reverse);
  } else {
    const int group_items_in_a_window = group_items_size / windows.size();
    auto* window = windows[focus_index / group_items_in_a_window];
    gfx::Point magnifier_origin;
    // Focus on the window, which is the first item inside the focusable items
    // of a window. Otherwise, advance the focus among the focusable items
    // of the camera preview inside the window.
    if (focus_index % group_items_in_a_window == 0) {
      // Remove the focus from previous focused item, which is always the
      // camera preview view for now regardless of reverse or not. As it is the
      // unique focusable item inside `kCameraPreview` focus group now.
      if (camera_preview_view)
        camera_preview_view->PseudoBlur();
      session_->HighlightWindowForTab(window);

      magnifier_origin = window->GetBoundsInScreen().origin();
    } else {
      DCHECK(camera_preview_view);
      // If `reverse`, highlight the window first to make sure the camera
      // preview will be shown inside `window`.
      if (reverse)
        session_->HighlightWindowForTab(window);
      camera_preview_view->PseudoFocus();

      magnifier_origin = camera_preview_view->GetBoundsInScreen().origin();
    }
    // TODO(afakhry): Check with a11y team if we need to focus on a
    // different region of the window.
    magnifier_utils::MaybeUpdateActiveMagnifierFocus(magnifier_origin);
  }
}

}  // namespace ash
