// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_RESOLUTION_NOTIFICATION_CONTROLLER_H_
#define ASH_DISPLAY_RESOLUTION_NOTIFICATION_CONTROLLER_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/display/display_change_dialog.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

FORWARD_DECLARE_TEST(DisplayPrefsTest, PreventStore);

// A class which manages the dialog displayed to notify the user that
// the display configuration has been changed.
class ASH_EXPORT ResolutionNotificationController
    : public display::DisplayObserver,
      public display::DisplayManagerObserver {
 public:
  ResolutionNotificationController();

  ResolutionNotificationController(const ResolutionNotificationController&) =
      delete;
  ResolutionNotificationController& operator=(
      const ResolutionNotificationController&) = delete;

  ~ResolutionNotificationController() override;

  // If |display_id| is not the internal display and |source| is |kSourceUser|
  // (which means user initiated the change), Prepare a resolution change
  // notification for |display_id| from |old_resolution| to |new_resolution|,
  // which offers a button to revert the change in case something goes wrong.
  // The dialog is not dismissed by the user, the change is reverted.
  //
  // Then call DisplayManager::SetDisplayMode() to apply the resolution change,
  // and return the result; true if success, false otherwise.
  // In case SetDisplayMode() fails, the prepared notification will be
  // discarded.
  //
  // If |display_id| is the internal display or |source| is |kSourcePolicy|, the
  // resolution change is applied directly without preparing the confirm/revert
  // notification (this kind of notification is only useful for external
  // displays).
  //
  // This method does not create a notification itself. The notification will be
  // created the next OnDisplayConfigurationChanged(), which will be called
  // asynchronously after the resolution change is requested by this method.
  //
  // |accept_callback| will be called when the user accepts the resoltion change
  // by closing the notification bubble or clicking on the accept button (if
  // any).
  [[nodiscard]] bool PrepareNotificationAndSetDisplayMode(
      int64_t display_id,
      const display::ManagedDisplayMode& old_resolution,
      const display::ManagedDisplayMode& new_resolution,
      crosapi::mojom::DisplayConfigSource source,
      base::OnceClosure accept_callback);

  DisplayChangeDialog* dialog_for_testing() const {
    return confirmation_dialog_.get();
  }

  bool ShouldShowDisplayChangeDialog() const;

 private:
  friend class ResolutionNotificationControllerTest;
  FRIEND_TEST_ALL_PREFIXES(ResolutionNotificationControllerTest, Timeout);
  FRIEND_TEST_ALL_PREFIXES(DisplayPrefsTest, PreventStore);

  // A struct to bundle the data for a single resolution change.
  struct ResolutionChangeInfo;

  // Create a new modal dialog, or replace the dialog if it already exists.
  void CreateOrReplaceModalDialog();

  // Called when the user accepts the display resolution change. Set
  // |close_notification| to true when the notification should be removed.
  void AcceptResolutionChange();

  // Called when the user wants to revert the display resolution change.
  void RevertResolutionChange(bool display_was_removed);

  // display::DisplayObserver overrides:
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;

  // display::DisplayManagerObserver overrides:
  void OnDidApplyDisplayChanges() override;

  std::unique_ptr<ResolutionChangeInfo> change_info_;

  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtr<DisplayChangeDialog> confirmation_dialog_;

  base::WeakPtrFactory<ResolutionNotificationController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_DISPLAY_RESOLUTION_NOTIFICATION_CONTROLLER_H_
