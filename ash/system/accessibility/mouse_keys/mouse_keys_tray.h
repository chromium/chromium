// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_TRAY_H_
#define ASH_SYSTEM_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_TRAY_H_

#include <string>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash {

class Shelf;

// A button in the tray that lets users start/stop mouse keys.
class ASH_EXPORT MouseKeysTray : public TrayBackgroundView,
                                 public AccessibilityObserver,
                                 public SessionObserver {
  METADATA_HEADER(MouseKeysTray, TrayBackgroundView)

 public:
  MouseKeysTray(Shelf* shelf, TrayBackgroundViewCatalogName catalog_name);
  MouseKeysTray(const MouseKeysTray&) = delete;
  MouseKeysTray& operator=(const MouseKeysTray&) = delete;
  ~MouseKeysTray() override;

  // TrayBackgroundView:
  void Initialize() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
  void HideBubble(const TrayBubbleView* bubble_view) override {}
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override {}
  void UpdateTrayItemColor(bool is_active) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  friend class MouseKeysTrayTest;

  views::ImageView* GetIcon();

  ScopedSessionObserver session_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_TRAY_H_
