// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_ADMIN_TEMPLATE_LAUNCH_TRACKER_H_
#define ASH_WM_DESKS_TEMPLATES_ADMIN_TEMPLATE_LAUNCH_TRACKER_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

class DeskTemplate;
class SavedDeskDelegate;

// Holds updates to apply to a window in an admin template.
struct AdminTemplateWindowUpdate {
  // The restore window id (RWID) of the window that is being updated. This is
  // the RWID as it appears in the originating template, not what the launched
  // window has since the latter has been made unique for each launch.
  int32_t template_rwid;

  // Optional new bounds for the window.
  std::optional<gfx::Rect> bounds;

  // Optional new display ID for the window.
  std::optional<int64_t> display_id;

  // Optional new Z index for the window, relative to the other tracked
  // windows.
  std::optional<int32_t> activation_index;
};

// Apply changes in `update` to `admin_template`. If `update` has specified a
// window that doesn't exist in the template, false is returned and the template
// is left unchanged.
ASH_EXPORT bool MergeAdminTemplateWindowUpdate(
    DeskTemplate& admin_template,
    const AdminTemplateWindowUpdate& update);

// Figures out where to place an admin template window. The passed `bounds` is
// first adjusted to fit inside `work_area` (note that this may change the
// size). It is then placed, if possible, so that it does not overlap exactly
// with any of the bounds in `existing_bounds`.
ASH_EXPORT void AdjustAdminTemplateWindowBounds(
    const gfx::Rect& work_area,
    const std::vector<gfx::Rect>& existing_bounds,
    gfx::Rect& bounds);

// Returns `window_count` bounds for windows automatically laid to fit
// `work_area_size`.
ASH_EXPORT std::vector<gfx::Rect> GetInitialWindowLayout(
    const gfx::Size& work_area_size,
    const int window_count);

// This class is used to launch an admin template and track the windows that
// have been launched from it.
class ASH_EXPORT AdminTemplateLaunchTracker {
 public:
  // Construct an admin template launch tracker. The passed `admin_template`
  // (which must not be null) will be updated as the user interacts with
  // launched windows. Updates to the template are sent to
  // `template_update_cb`. Updates are held (and merged) for `update_delay` so
  // that rapid window changes do not result in a deluge of callback calls.
  AdminTemplateLaunchTracker(
      std::unique_ptr<DeskTemplate> admin_template,
      base::RepeatingCallback<void(const DeskTemplate&)> template_update_cb,
      base::TimeDelta update_delay);

  AdminTemplateLaunchTracker(const AdminTemplateLaunchTracker&) = delete;
  AdminTemplateLaunchTracker& operator=(const AdminTemplateLaunchTracker&) =
      delete;

  // Destroys the launch tracker, as well as all internal window observers. The
  // update callback will not be invoked after the tracker has been destroyed.
  ~AdminTemplateLaunchTracker();

  // Sets up window observers for windows that are expected to be launched. It
  // then launches the template using `delegate`.
  void LaunchTemplate(SavedDeskDelegate* delegate, int64_t default_display_id);

  // If there is an existing pending update to this template, it will be
  // dispatched using the update callback. If there are no pending updates, then
  // this is a no-op.
  void FlushPendingUpdate();

  // Returns true if there are launched windows from this tracker that are still
  // open. When this returns false, there are no more windows that can generate
  // updates. Note that there may still be a pending update, so
  // `FlushPendingUpdate` should typically be called before the tracker is
  // destroyed.
  bool IsActive() const;

 private:
  // Called when an observer is created (either a desk or window observer).
  void OnObserverCreated(std::unique_ptr<base::CheckedObserver> observer);

  // Called when an observer is done (the observee has been destroyed).
  void OnObserverDone(base::CheckedObserver* observer);

  // Called when an observed window has changed.
  void OnUpdate(const AdminTemplateWindowUpdate& update);

  // Called when it's time to fire off a delayed callback.
  void OnTimer();

  // The template that will be updated based on received events. Changes are
  // eventually saved to the storage model.
  std::unique_ptr<DeskTemplate> admin_template_;

  // Callback that will be invoked when the template has been updated.
  base::RepeatingCallback<void(const DeskTemplate&)> template_update_cb_;

  // Window observers launched by the tracker. They are tracked so that they can
  // be destroyed, if the launchtracker itself is destroyed.
  std::vector<std::unique_ptr<base::CheckedObserver>> window_observers_;

  // Calls to the update callback are delayed by this much.
  base::TimeDelta update_delay_;

  // Timer used for callback delays.
  base::OneShotTimer update_delay_timer_;

  base::WeakPtrFactory<AdminTemplateLaunchTracker> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_ADMIN_TEMPLATE_LAUNCH_TRACKER_H_
