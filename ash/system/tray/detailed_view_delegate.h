// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_DETAILED_VIEW_DELEGATE_H_
#define ASH_SYSTEM_TRAY_DETAILED_VIEW_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class UnifiedSystemTrayController;

// A delegate of TrayDetailedView that handles bubble related actions e.g.
// transition to the main view, closing the bubble, etc.
class ASH_EXPORT DetailedViewDelegate {
 public:
  explicit DetailedViewDelegate(UnifiedSystemTrayController* tray_controller);

  DetailedViewDelegate(const DetailedViewDelegate&) = delete;
  DetailedViewDelegate& operator=(const DetailedViewDelegate&) = delete;

  virtual ~DetailedViewDelegate();

  // Transition to the main view from the detailed view. |restore_focus| is true
  // if the title row has keyboard focus before transition. If so, the main view
  // should focus on the corresponding element of the detailed view.
  virtual void TransitionToMainView(bool restore_focus);

  // Close the bubble that contains the detailed view.
  virtual void CloseBubble();

  // Returns the margin around the scroll view. Most detailed views should use
  // the default implementation. Shelf pods that reuse detailed views may need
  // custom margins.
  virtual gfx::Insets GetScrollViewMargin() const;

  // Return the back button used in the title row. Caller takes ownership of the
  // returned view.
  virtual views::Button* CreateBackButton(
      views::Button::PressedCallback callback);

  // Return the info button used in the title row. Caller takes ownership of the
  // returned view.
  virtual views::Button* CreateInfoButton(
      views::Button::PressedCallback callback,
      int info_accessible_name_id);

  // Return the settings button used in the title row. Caller takes ownership of
  // the returned view.
  virtual views::Button* CreateSettingsButton(
      views::Button::PressedCallback callback,
      int setting_accessible_name_id);

  // Return the help button used in the title row. Caller takes ownership of the
  // returned view.
  virtual views::Button* CreateHelpButton(
      views::Button::PressedCallback callback);

 private:
  const raw_ptr<UnifiedSystemTrayController> tray_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_DETAILED_VIEW_DELEGATE_H_
