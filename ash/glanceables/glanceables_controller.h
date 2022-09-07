// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_CONTROLLER_H_
#define ASH_GLANCEABLES_GLANCEABLES_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/time/time.h"
#include "ui/wm/public/activation_change_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class GlanceablesDelegate;
class GlanceablesView;
class GlanceablesWindowHider;

// Controls the "welcome back" glanceables screen shown on login.
class ASH_EXPORT GlanceablesController : public wm::ActivationChangeObserver,
                                         public TabletModeObserver {
 public:
  GlanceablesController();
  GlanceablesController(const GlanceablesController&) = delete;
  GlanceablesController& operator=(const GlanceablesController&) = delete;
  ~GlanceablesController() override;

  // Initializes the controller and sets the delegate.
  void Init(std::unique_ptr<GlanceablesDelegate> delegate);

  // Creates the UI and starts fetching data.
  void ShowOnLogin();

  // Shows from the UI affordance in overview mode / desks bar.
  void ShowFromOverview();

  // Returns true if the glanceables screen is showing.
  bool IsShowing() const;

  // Creates the glanceables widget and view.
  void CreateUi();

  // Destroys the glanceables widget and view.
  void DestroyUi();

  // Triggers a session restore.
  void RestoreSession();

  // Returns true if a signout screenshot should be taken for this session.
  bool ShouldTakeSignoutScreenshot() const;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_focus,
                         aura::Window* lost_focus) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;

 private:
  friend class GlanceablesTest;

  // Triggers a fetch of data from the server.
  void FetchData();

  // Adds blur to `widget_` and semiopaque black background to `view_`.
  // TODO(crbug.com/1354343): investigate if there's a more efficient way to do
  // this.
  void ApplyBackdrop() const;

  std::unique_ptr<GlanceablesDelegate> delegate_;
  std::unique_ptr<views::Widget> widget_;
  GlanceablesView* view_ = nullptr;
  bool show_session_restore_ = true;

  // Hides windows while glanceables are showing.
  std::unique_ptr<GlanceablesWindowHider> window_hider_;

  // The start of current month in UTC. Used for fetching calendar events.
  // TODO(crbug.com/1353495): Update value at the beginning of the next month
  // and trigger another fetch.
  base::Time start_of_month_utc_;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_CONTROLLER_H_
