// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_OVERVIEW_OVERVIEW_BUTTON_TRAY_H_
#define ASH_SYSTEM_OVERVIEW_OVERVIEW_BUTTON_TRAY_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shelf/shelf.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_constants.h"

namespace ui {
class Event;
class GestureEvent;
}  // namespace ui

namespace views {
class ImageView;
}

namespace ash {

// Status area tray for showing a toggle for Overview Mode. Overview Mode
// is equivalent to OverviewController being in selection mode.
// This hosts a ShellObserver that listens for the activation of Maximize Mode
// This tray will only be visible while in this state. This tray does not
// provide any bubble view windows.
class ASH_EXPORT OverviewButtonTray : public TrayBackgroundView,
                                      public SessionObserver,
                                      public OverviewObserver,
                                      public TabletModeObserver,
                                      public ShelfConfig::Observer {
  METADATA_HEADER(OverviewButtonTray, TrayBackgroundView)

 public:
  // Second taps within this time will be counted as double taps. Use this
  // instead of ui::Event's click_count and tap_count as those have a minimum
  // time bewtween events before the second tap counts as a double tap.
  // TODO(crbug.com/40565331): We should the gesture detector double tap time or
  // overview enter animation time, once ux decides which one to match (both are
  // 300ms currently).
  static constexpr base::TimeDelta kDoubleTapThresholdMs =
      base::Milliseconds(300);

  explicit OverviewButtonTray(Shelf* shelf);
  OverviewButtonTray(const OverviewButtonTray&) = delete;
  OverviewButtonTray& operator=(const OverviewButtonTray&) = delete;
  ~OverviewButtonTray() override;

  // Sets the ink drop ripple to ACTIVATED immediately with no animations.
  void SnapRippleToActivated();

  // views::Button:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnded() override;

  // TabletModeObserver:
  void OnTabletModeEventsBlockingChanged() override;

  // ShelfConfigObserver:
  void OnShelfConfigUpdated() override;

  // TrayBackgroundView:
  void UpdateAfterLoginStatusChange() override;
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  void UpdateTrayItemColor(bool is_active) override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void OnThemeChanged() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

 private:
  friend class OverviewButtonTrayTest;

  // Callback called when this is pressed. Long press is reacted to in
  // `OnGestureEvent()`, see crbug/1374368.
  void OnButtonPressed(const ui::Event& event);

  void UpdateIconVisibility();

  // Gets the icon image of `icon_`.
  gfx::ImageSkia GetIconImage();

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  raw_ptr<views::ImageView> icon_;

  ScopedSessionObserver scoped_session_observer_;

  // Stores the timestamp of the last tap event time that happened while not
  // in overview mode. Used to check for double taps, which invoke quick switch.
  std::optional<base::TimeTicks> last_press_event_time_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_OVERVIEW_OVERVIEW_BUTTON_TRAY_H_
