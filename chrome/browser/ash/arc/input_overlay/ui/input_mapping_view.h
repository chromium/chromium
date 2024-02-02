// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MAPPING_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MAPPING_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector_observer.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class Action;
class DisplayOverlayController;

// InputMappingView shows all the input mappings.
class InputMappingView : public views::View, public TouchInjectorObserver {
  METADATA_HEADER(InputMappingView, views::View)

 public:
  explicit InputMappingView(
      DisplayOverlayController* display_overlay_controller);
  InputMappingView(const InputMappingView&) = delete;
  InputMappingView& operator=(const InputMappingView&) = delete;
  ~InputMappingView() override;

  void SetDisplayMode(const DisplayMode mode);

 private:
  void ProcessPressedEvent(const ui::LocatedEvent& event);

  // Reorder the child views to have focus order as:
  // If the window aspect-ratio > 1
  // - First, focus the views on the left half of the window from top to bottom.
  // - Then focus the views on the right half of the window from top to bottom.
  // If the window aspect-ratio <= 1
  // - Focus from top to bottom.
  void SortChildren();

  // Adds the action without opening the ButtonOptionsMenu.
  void OnActionAddedInternal(Action& action);

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // TouchInjectorObserver:
  void OnActionAdded(Action& action) override;
  void OnActionRemoved(const Action& action) override;
  void OnActionTypeChanged(Action* action, Action* new_action) override;
  void OnActionInputBindingUpdated(const Action& action) override;
  void OnContentBoundsSizeChanged() override;
  void OnActionNewStateRemoved(const Action& action) override;

  const raw_ptr<DisplayOverlayController> controller_ = nullptr;
  DisplayMode current_display_mode_ = DisplayMode::kNone;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MAPPING_VIEW_H_
