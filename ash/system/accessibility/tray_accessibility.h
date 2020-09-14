// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_TRAY_ACCESSIBILITY_H_
#define ASH_SYSTEM_ACCESSIBILITY_TRAY_ACCESSIBILITY_H_

#include <stdint.h>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/macros.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace chromeos {
class TrayAccessibilityTest;
}

namespace views {
class Button;
class View;
}  // namespace views

namespace ash {
class HoverHighlightView;
class DetailedViewDelegate;
class TrayAccessibilityLoginScreenTest;
class TrayAccessibilityTest;

namespace tray {

// Create the detailed view of accessibility tray.
class ASH_EXPORT AccessibilityDetailedView : public TrayDetailedView {
 public:
  static constexpr char kClassName[] = "AccessibilityDetailedView";

  explicit AccessibilityDetailedView(DetailedViewDelegate* delegate);
  ~AccessibilityDetailedView() override {}

  void OnAccessibilityStatusChanged();

  // views::View
  const char* GetClassName() const override;

 private:
  friend class ::ash::TrayAccessibilityLoginScreenTest;
  friend class ::ash::TrayAccessibilityTest;
  friend class chromeos::TrayAccessibilityTest;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void HandleButtonPressed(views::Button* sender,
                           const ui::Event& event) override;
  void CreateExtraTitleRowButtons() override;

  // Launches the WebUI settings in a browser and closes the system menu.
  void ShowSettings();

  // Launches the a11y help article in a browser and closes the system menu.
  void ShowHelp();

  // Add the accessibility feature list.
  void AppendAccessibilityList();

  HoverHighlightView*
      feature_views_[AccessibilityControllerImpl::FeatureType::kFeatureCount] =
          {nullptr};
  views::Button* help_view_ = nullptr;
  views::Button* settings_view_ = nullptr;

  // These exist for tests. The canonical state is stored in prefs.
  bool features_enabled_[AccessibilityControllerImpl::kFeatureCount] = {false};

  LoginStatus login_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_TRAY_ACCESSIBILITY_H_
