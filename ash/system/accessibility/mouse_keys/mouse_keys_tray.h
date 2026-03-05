// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_TRAY_H_
#define ASH_SYSTEM_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_TRAY_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/imaged_tray_icon.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class Shelf;

// A button in the tray that lets users start/stop mouse keys.
class ASH_EXPORT MouseKeysTray : public ImagedTrayIcon,
                                 public AccessibilityObserver,
                                 public SessionObserver {
  METADATA_HEADER(MouseKeysTray, ImagedTrayIcon)

 public:
  MouseKeysTray(Shelf* shelf, TrayBackgroundViewCatalogName catalog_name);
  MouseKeysTray(const MouseKeysTray&) = delete;
  MouseKeysTray& operator=(const MouseKeysTray&) = delete;
  ~MouseKeysTray() override;

  // TrayBackgroundView:
  void Initialize() override;

  // ImagedTrayIcon:
  void HandleLocaleChange() override;
  void UpdateTrayItemColor(bool is_active) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  base::WeakPtr<MouseKeysTray> GetWeakPtr();
  void SetMouseKeysStatusText(bool is_active);
  void UpdateStatus();

 private:
  friend class MouseKeysTrayTest;

  ScopedSessionObserver session_observer_{this};

  // Callback that's called when they tray is pressed.
  void OnMouseKeyIconPressed(const ui::Event& event);

  base::WeakPtrFactory<MouseKeysTray> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_TRAY_H_
