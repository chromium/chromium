// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_TRAY_H_
#define ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_TRAY_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "ui/views/controls/image_view.h"

namespace views {
class ImageView;
}

namespace ash {

// A button in the tray that lets users start/stop Select-to-Speak.
class ASH_EXPORT SelectToSpeakTray : public TrayBackgroundView,
                                     public AccessibilityObserver,
                                     public SessionObserver {
 public:
  SelectToSpeakTray(Shelf* shelf, TrayBackgroundViewCatalogName catalog_name);

  SelectToSpeakTray(const SelectToSpeakTray&) = delete;
  SelectToSpeakTray& operator=(const SelectToSpeakTray&) = delete;

  ~SelectToSpeakTray() override;

  // TrayBackgroundView:
  void Initialize() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  const char* GetClassName() const override;
  bool PerformAction(const ui::Event& event) override;
  void OnThemeChanged() override;
  // The SelectToSpeakTray does not have a bubble, so these functions are
  // no-ops.
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
  void ClickedOutsideBubble() override {}

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  friend class SelectToSpeakTrayTest;

  // Updates icon depending on the current status of select-to-speak. And
  // updates the visibility of the tray depending on whether select-to-speak is
  // enabled or disabled.
  void UpdateIconOnCurrentStatus();

  // Updates icon if the color of the icon changes.
  void UpdateIconOnColorChanges();

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;

  ScopedSessionObserver session_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_TRAY_H_
