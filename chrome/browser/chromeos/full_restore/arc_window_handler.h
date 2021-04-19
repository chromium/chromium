// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_WINDOW_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_WINDOW_HANDLER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/wm_helper.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace exo {
class ClientControlledShellSurface;
}

namespace chromeos {
namespace full_restore {

// Returns true if the ARC supports ghost window.
bool IsArcGhostWindowEnabled();

// Returns window info compatible with ARC. If the window bounds is not
// appropriate for the display, it will be removed.
//
// The app window bounds can be decided if and only if it matches the
// conditions:
//   1. The |display_id| still exists on system.
//   2. Previous ARC app window bounds on display is recorded.
// Otherwise returns null.
apps::mojom::WindowInfoPtr HandleArcWindowInfo(
    apps::mojom::WindowInfoPtr window_info);

// The ArcWindowHandler class provides control for ARC ghost window.
class ArcWindowHandler {
  // Map from window_session_id to exo::ClientControlledShellSurface.
  using ShellSurfaceMap =
      std::map<int, std::unique_ptr<exo::ClientControlledShellSurface>>;

  // This class populates the exo::ShellSurfaceBase to PropertyHandler by
  // the corresponding window session id.
  class WindowSessionResolver : public exo::WMHelper::AppPropertyResolver {
   public:
    explicit WindowSessionResolver(ShellSurfaceMap* session_id_map);
    WindowSessionResolver(const WindowSessionResolver&) = delete;
    WindowSessionResolver& operator=(const WindowSessionResolver&) = delete;
    ~WindowSessionResolver() override = default;

    // exo::WMHelper::AppPropertyResolver:
    void PopulateProperties(
        const Params& params,
        ui::PropertyHandler& out_properties_container) override;

   private:
    ShellSurfaceMap* session_id_map_;
  };

  // This class is used to notify observers that AppInstance is connected.
  class Observer : public base::CheckedObserver {
   public:
    // Observer for app instance connection ready.
    virtual void OnAppInstanceConnected() {}

   protected:
    ~Observer() override = default;
  };

 public:
  ArcWindowHandler();
  ArcWindowHandler(const ArcWindowHandler&) = delete;
  ArcWindowHandler& operator=(const ArcWindowHandler&) = delete;
  ~ArcWindowHandler();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer);

  void OnAppInstanceConnected();

 private:
  // Map window session id to ClientControlledShellSurface.
  ShellSurfaceMap session_id_to_shell_surface_;

  base::ObserverList<Observer> observer_list_;
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_WINDOW_HANDLER_H_
