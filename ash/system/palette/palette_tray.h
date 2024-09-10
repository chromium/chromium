// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_PALETTE_TRAY_H_
#define ASH_SYSTEM_PALETTE_PALETTE_TRAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/palette/palette_tool_manager.h"
#include "ash/system/palette/stylus_battery_delegate.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/events/devices/input_device_event_observer.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace gfx {
class Point;
}

namespace ui {
class Event;
class EventHandler;
class TouchEvent;
}  // namespace ui

namespace views {
class ImageView;
class Widget;
}  // namespace views

namespace ash {

class PaletteTrayTestApi;
class PaletteToolManager;
class PaletteWelcomeBubble;
class Shelf;
class TrayBubbleView;
class TrayBubbleWrapper;

// The PaletteTray shows the palette in the bottom area of the screen. This
// class also controls the lifetime for all of the tools available in the
// palette. PaletteTray has one instance per-display. It is only made visible if
// the display has stylus hardware.
class ASH_EXPORT PaletteTray : public TrayBackgroundView,
                               public SessionObserver,
                               public ShelfObserver,
                               public ShellObserver,
                               public display::DisplayManagerObserver,
                               public PaletteToolManager::Delegate,
                               public ui::InputDeviceEventObserver,
                               public ProjectorSessionObserver {
  METADATA_HEADER(PaletteTray, TrayBackgroundView)

 public:
  explicit PaletteTray(Shelf* shelf);

  PaletteTray(const PaletteTray&) = delete;
  PaletteTray& operator=(const PaletteTray&) = delete;

  ~PaletteTray() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns true if the palette tray contains the given point. This is useful
  // for determining if an event should be propagated through to the palette.
  bool ContainsPointInScreen(const gfx::Point& point);

  // Returns true if the palette should be visible in the UI. This happens when
  // there is a stylus display and the user has not disabled it in settings.
  // This can be overridden by passing switches.
  bool ShouldShowPalette() const;

  // Handles stylus events to show the welcome bubble on first usage.
  void OnStylusEvent(const ui::TouchEvent& event);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // ShellObserver:
  void OnLockStateChanged(bool locked) override;
  void OnShellInitialized() override;
  void OnShellDestroying() override;

  // display::DisplayManagerObserver:
  void OnDidApplyDisplayChanges() override;

  // TrayBackgroundView:
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  void UpdateTrayItemColor(bool is_active) override;
  void OnThemeChanged() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void Initialize() override;
  void CloseBubbleInternal() override;
  void ShowBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;

  // PaletteToolManager::Delegate:
  void HidePalette() override;
  void HidePaletteImmediately() override;

  // ProjectorSessionObserver:
  void OnProjectorSessionActiveStateChanged(bool active) override;

 private:
  friend class PaletteTrayTestApi;
  friend class StatusAreaInternalsHandler;

  // ui::InputDeviceObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;
  void OnStylusStateChanged(ui::StylusState stylus_state) override;
  void OnTouchDeviceAssociationChanged() override;

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // PaletteToolManager::Delegate:
  void OnActiveToolChanged() override;
  aura::Window* GetWindow() override;

  // Returns true if we're on a display with a stylus or on every
  // display if requested from the command line.
  bool ShouldShowOnDisplay();

  // Returns true if our widget is on an internal display.
  bool IsWidgetOnInternalDisplay();

  // Initializes with Shell's local state and starts to observe it.
  void InitializeWithLocalState();

  // Updates the tray icon from the palette tool manager.
  void UpdateTrayIcon();

  // Sets the icon to visible if the palette can be used.
  void UpdateIconVisibility();

  // Called when the palette enabled pref has changed.
  void OnPaletteEnabledPrefChanged();

  // Callback called when this TrayBackgroundView is pressed.
  void OnPaletteTrayPressed(const ui::Event& event);

  // Called when the has seen stylus pref has changed.
  void OnHasSeenStylusPrefChanged();

  // Deactivates the active tool. Returns false if there was no active tool.
  bool DeactivateActiveTool();

  // Helper method which returns true if the device has seen a stylus event
  // previously, or if the device has an internal stylus.
  bool HasSeenStylus();

  // Have the palette act as though it is on a display with a stylus for
  // testing purposes.
  void SetDisplayHasStylusForTesting();

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;

  std::unique_ptr<PaletteToolManager> palette_tool_manager_;
  std::unique_ptr<PaletteWelcomeBubble> welcome_bubble_;
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // A Shell pre-target handler that notifies PaletteTray of stylus events.
  std::unique_ptr<ui::EventHandler> stylus_event_handler_;

  raw_ptr<PrefService> local_state_ = nullptr;               // Not owned.
  raw_ptr<PrefService> active_user_pref_service_ = nullptr;  // Not owned.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_local_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_user_;

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  raw_ptr<views::ImageView> icon_ = nullptr;

  // Cached palette pref value.
  bool is_palette_enabled_ = true;

  // True when the palette tray should not be visible, regardless of palette
  // pref values.
  bool is_palette_visibility_paused_ = false;

  // Whether the palette should behave as though its display has a stylus.
  bool display_has_stylus_for_testing_ = false;

  // Used to indicate whether the palette bubble is automatically opened by a
  // stylus eject event.
  bool is_bubble_auto_opened_ = false;

  // Number of actions in pen palette bubble.
  int num_actions_in_bubble_ = 0;

  base::ScopedObservation<ProjectorSession, ProjectorSessionObserver>
      projector_session_observation_{this};

  ScopedSessionObserver scoped_session_observer_;

  base::WeakPtrFactory<PaletteTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_PALETTE_TRAY_H_
