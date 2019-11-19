// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_PALETTE_TRAY_H_
#define ASH_SYSTEM_PALETTE_PALETTE_TRAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/palette/palette_tool_manager.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/devices/input_device_event_observer.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace gfx {
class Point;
}

namespace views {
class ImageView;
}

namespace ash {

class PaletteTrayTestApi;
class PaletteToolManager;
class PaletteWelcomeBubble;
class TrayBubbleWrapper;

// The PaletteTray shows the palette in the bottom area of the screen. This
// class also controls the lifetime for all of the tools available in the
// palette. PaletteTray has one instance per-display. It is only made visible if
// the display is primary and if the device has stylus hardware.
class ASH_EXPORT PaletteTray : public TrayBackgroundView,
                               public SessionObserver,
                               public ShellObserver,
                               public PaletteToolManager::Delegate,
                               public ui::InputDeviceEventObserver {
 public:
  explicit PaletteTray(Shelf* shelf);
  ~PaletteTray() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns true if the palette tray contains the given point. This is useful
  // for determining if an event should be propagated through to the palette.
  bool ContainsPointInScreen(const gfx::Point& point);

  // Returns true if the palette should be visible in the UI. This happens when:
  // there is a stylus input, there is an internal display, and the user has not
  // disabled it in settings. This can be overridden by passing switches.
  bool ShouldShowPalette() const;

  // Handles stylus events to show the welcome bubble on first usage.
  void OnStylusEvent(const ui::TouchEvent& event);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // ShellObserver:
  void OnLockStateChanged(bool locked) override;

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void Initialize() override;
  bool PerformAction(const ui::Event& event) override;
  void CloseBubble() override;
  void ShowBubble(bool show_by_click) override;
  TrayBubbleView* GetBubbleView() override;
  const char* GetClassName() const override;

  // PaletteToolManager::Delegate:
  void HidePalette() override;
  void HidePaletteImmediately() override;
  void RecordPaletteOptionsUsage(PaletteTrayOptions option,
                                 PaletteInvocationMethod method) override;
  void RecordPaletteModeCancellation(PaletteModeCancelType type) override;

 private:
  friend class PaletteTrayTestApi;

  // ui::InputDeviceObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;
  void OnStylusStateChanged(ui::StylusState stylus_state) override;

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  base::string16 GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // PaletteToolManager::Delegate:
  void OnActiveToolChanged() override;
  aura::Window* GetWindow() override;

  // Initializes with Shell's local state and starts to observe it.
  void InitializeWithLocalState();

  // Updates the tray icon from the palette tool manager.
  void UpdateTrayIcon();

  // Sets the icon to visible if the palette can be used.
  void UpdateIconVisibility();

  // Called when the palette enabled pref has changed.
  void OnPaletteEnabledPrefChanged();

  // Called when the has seen stylus pref has changed.
  void OnHasSeenStylusPrefChanged();

  // Deactivates the active tool. Returns false if there was no active tool.
  bool DeactivateActiveTool();

  // Helper method which returns true if the device has seen a stylus event
  // previously, or if the device has an internal stylus.
  bool HasSeenStylus();

  std::unique_ptr<PaletteToolManager> palette_tool_manager_;
  std::unique_ptr<PaletteWelcomeBubble> welcome_bubble_;
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // A Shell pre-target handler that notifies PaletteTray of stylus events.
  std::unique_ptr<ui::EventHandler> stylus_event_handler_;

  PrefService* local_state_ = nullptr;               // Not owned.
  PrefService* active_user_pref_service_ = nullptr;  // Not owned.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_local_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_user_;

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;

  // Cached palette pref value.
  bool is_palette_enabled_ = true;

  // Used to indicate whether the palette bubble is automatically opened by a
  // stylus eject event.
  bool is_bubble_auto_opened_ = false;

  // Number of actions in pen palette bubble.
  int num_actions_in_bubble_ = 0;

  ScopedSessionObserver scoped_session_observer_;

  base::WeakPtrFactory<PaletteTray> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PaletteTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_PALETTE_TRAY_H_
