// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/admin_template_launch_tracker.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/templates/saved_desk_constants.h"
#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/scoped_observation.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

namespace {

using WindowUpdateCallback =
    base::RepeatingCallback<void(const AdminTemplateWindowUpdate&)>;
using ObserverCreatedCallback =
    base::RepeatingCallback<void(std::unique_ptr<base::CheckedObserver>)>;
using ObserverDoneCallback =
    base::RepeatingCallback<void(base::CheckedObserver*)>;

// The next activation index to assign to an admin template window.
int32_t g_admin_template_next_activation_index =
    kAdminTemplateStartingActivationIndex;

// This function updates the activation indices of all the windows in an admin
// template so that windows launched from it will stack in the order they are
// defined, while also stacking on top of any existing windows.
void UpdateAdminTemplateActivationIndices(DeskTemplate& saved_desk) {
  auto& app_id_to_launch_list =
      saved_desk.mutable_desk_restore_data()->mutable_app_id_to_launch_list();
  // Go through the windows as defined in the saved desk in reverse order so
  // that the window with the lowest id gets the lowest activation index. NB:
  // for now, we expect admin templates to only contain a single app.
  for (auto& [app_id, launch_list] : app_id_to_launch_list) {
    for (auto& [window_id, app_restore_data] : base::Reversed(launch_list)) {
      app_restore_data->activation_index =
          g_admin_template_next_activation_index--;
    }
  }
}

// Holds a pair of restore window identifiers.
struct WindowIdPair {
  // The RWID as it occurs in the original template.
  int32_t template_rwid;
  // The corresponding RWID after having been made unique for a launch.
  int32_t unique_rwid;
};

// `AdminTemplateWindowObserver` observes a window launched from an admin
// template and feeds this information back to the SavedDeskController.
class AdminTemplateWindowObserver : public aura::WindowObserver {
 public:
  AdminTemplateWindowObserver(WindowUpdateCallback update_cb,
                              ObserverDoneCallback done_cb,
                              aura::Window* window,
                              int32_t template_rwid)
      : update_cb_(std::move(update_cb)),
        done_cb_(std::move(done_cb)),
        template_rwid_(template_rwid) {
    observer_.Observe(window);
  }

 private:
  // aura::WindowObserver:
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override {
    if (parent) {
      if (aura::Window* root = parent->GetRootWindow()) {
        if (RootWindowSettings* settings = GetRootWindowSettings(root)) {
          // The window has been moved to a new display.
          update_cb_.Run({.template_rwid = template_rwid_,
                          .display_id = settings->display_id});
        }
      }
    }
  }

  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    update_cb_.Run({.template_rwid = template_rwid_, .bounds = new_bounds});
  }

  void OnWindowDestroyed(aura::Window* window) override { done_cb_.Run(this); }

  // Called when the the tracked window has updated its state.
  WindowUpdateCallback update_cb_;
  // Called when the tracked window has been destroyed. This destroys the
  // observer.
  ObserverDoneCallback done_cb_;
  // The window ID of the tracked window, as it appears in the template.
  int32_t template_rwid_;

  base::ScopedObservation<aura::Window, aura::WindowObserver> observer_{this};
};

// `AdminTemplateDeskObserver` observes window creation on a desk and waits
// until all windows from an expected set have been created.
class AdminTemplateDeskObserver : public aura::WindowObserver {
 public:
  AdminTemplateDeskObserver(WindowUpdateCallback update_cb,
                            ObserverCreatedCallback created_cb,
                            ObserverDoneCallback done_cb,
                            aura::Window* desk_container,
                            std::vector<WindowIdPair> rwids)
      : update_cb_(std::move(update_cb)),
        created_cb_(std::move(created_cb)),
        done_cb_(std::move(done_cb)),
        rwids_(std::move(rwids)) {
    observer_.Observe(desk_container);
  }

 private:
  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override {
    int32_t rwid = new_window->GetProperty(app_restore::kRestoreWindowIdKey);
    if (auto it = base::ranges::find(rwids_, rwid, &WindowIdPair::unique_rwid);
        it != rwids_.end()) {
      auto window_observer = std::make_unique<AdminTemplateWindowObserver>(
          update_cb_, done_cb_, new_window, it->template_rwid);

      created_cb_.Run(std::move(window_observer));

      rwids_.erase(it);
      if (rwids_.empty()) {
        // We have seen all windows that we expected to see. We will notify the
        // tracker of this. Note that this will cause this observer to be
        // deleted, so it's effectively a `delete this`.
        done_cb_.Run(this);
      }
    }
  }

  void OnWindowDestroyed(aura::Window* window) override { done_cb_.Run(this); }

  // TODO(dandersson): Consider if there's an edge case where a desk can be
  // closed between the launch has been initiated and windows appear.

  // Callback used by AdminTemplateWindowObserver instances.
  WindowUpdateCallback update_cb_;
  // Callback used when a new observer has been created.
  ObserverCreatedCallback created_cb_;
  // Callback used when an observer is done.
  ObserverDoneCallback done_cb_;
  // Restore window IDs of windows that have not yet been created.
  std::vector<WindowIdPair> rwids_;

  base::ScopedObservation<aura::Window, aura::WindowObserver> observer_{this};
};

// Returns a pointer to the `AppRestoreData` in `admin_template` for
// `template_rwid`, or null if the window doesn't exist.
app_restore::AppRestoreData* GetAppRestoreData(DeskTemplate& admin_template,
                                               int32_t template_rwid) {
  auto& app_id_to_launch_list = admin_template.mutable_desk_restore_data()
                                    ->mutable_app_id_to_launch_list();
  for (auto& [app_id, launch_list] : app_id_to_launch_list) {
    for (auto& [window_id, app_restore_data] : launch_list) {
      if (window_id == template_rwid) {
        return app_restore_data.get();
      }
    }
  }
  return nullptr;
}

}  // namespace

bool MergeAdminTemplateWindowUpdate(DeskTemplate& admin_template,
                                    const AdminTemplateWindowUpdate& update) {
  auto* restore_data = GetAppRestoreData(admin_template, update.template_rwid);
  if (!restore_data) {
    return false;
  }

  if (update.display_id) {
    restore_data->display_id = *update.display_id;
  }
  if (update.bounds) {
    restore_data->current_bounds = *update.bounds;
  }

  return true;
}

AdminTemplateLaunchTracker::AdminTemplateLaunchTracker(
    std::unique_ptr<DeskTemplate> admin_template,
    base::RepeatingCallback<void(const DeskTemplate&)> template_update_cb)
    : admin_template_(std::move(admin_template)),
      template_update_cb_(template_update_cb) {}

AdminTemplateLaunchTracker::~AdminTemplateLaunchTracker() = default;

void AdminTemplateLaunchTracker::LaunchTemplate(SavedDeskDelegate* delegate,
                                                int64_t default_display_id) {
  // Clone the template. This is done because we need to modify it for
  // launching, but those changes should not go into storage.
  auto admin_template = admin_template_->Clone();

  // Figure out which display we're going to launch windows into if the template
  // hasn't specified any, or the template refers to a display that isn't
  // available.
  aura::Window* default_root =
      Shell::GetRootWindowForDisplayId(default_display_id);
  if (!default_root) {
    default_root = Shell::GetPrimaryRootWindow();
    default_display_id = GetRootWindowSettings(default_root)->display_id;
  }

  // Set apps to launch on the current desk.
  auto* desks_controller = DesksController::Get();
  const int desk_index =
      desks_controller->GetDeskIndex(desks_controller->active_desk());
  admin_template->SetDeskIndex(desk_index);

  UpdateAdminTemplateActivationIndices(*admin_template);

  // Get a mapping from unique RWIDs to IDs as specified in the original
  // template. This is needed so that we can track launched windows and map
  // their changes back to the original template.
  auto mapping = admin_template->mutable_desk_restore_data()
                     ->MakeWindowIdsUniqueForDeskTemplate();

  // Maps root windows to a list of restore window IDs for the display.
  base::flat_map<aura::Window*, std::vector<WindowIdPair>> root_to_rwids;

  auto& app_id_to_launch_list = admin_template->mutable_desk_restore_data()
                                    ->mutable_app_id_to_launch_list();
  for (auto& [app_id, launch_list] : app_id_to_launch_list) {
    for (auto& [window_id, app_restore_data] : launch_list) {
      int64_t display_id =
          app_restore_data->display_id.value_or(default_display_id);

      // If the display doesn't exist, fall back.
      aura::Window* root = Shell::GetRootWindowForDisplayId(display_id);
      if (!root) {
        display_id = default_display_id;
        root = default_root;
      }

      app_restore_data->display_id = display_id;
      root_to_rwids[root].push_back(
          {.template_rwid = mapping[window_id], .unique_rwid = window_id});
    }
  }

  WindowUpdateCallback update_cb = base::BindRepeating(
      &AdminTemplateLaunchTracker::OnUpdate, weak_ptr_factory_.GetWeakPtr());
  ObserverCreatedCallback created_cb =
      base::BindRepeating(&AdminTemplateLaunchTracker::OnObserverCreated,
                          weak_ptr_factory_.GetWeakPtr());
  ObserverDoneCallback done_cb =
      base::BindRepeating(&AdminTemplateLaunchTracker::OnObserverDone,
                          weak_ptr_factory_.GetWeakPtr());

  // Set up launch trackers for all displays.
  for (auto& [root, rwids] : root_to_rwids) {
    aura::Window* container =
        desks_controller->active_desk()->GetDeskContainerForRoot(root);

    window_observers_.push_back(std::make_unique<AdminTemplateDeskObserver>(
        update_cb, created_cb, done_cb, container, std::move(rwids)));
  }

  // Finally, launch the apps.
  delegate->LaunchAppsFromSavedDesk(std::move(admin_template));
}

void AdminTemplateLaunchTracker::OnObserverCreated(
    std::unique_ptr<base::CheckedObserver> observer) {
  window_observers_.push_back(std::move(observer));
}

void AdminTemplateLaunchTracker::OnObserverDone(
    base::CheckedObserver* observer) {
  base::EraseIf(window_observers_,
                [&](const auto& ptr) { return ptr.get() == observer; });
}

void AdminTemplateLaunchTracker::OnUpdate(
    const AdminTemplateWindowUpdate& update) {
  // TODO(dandersson): Implement throttling of updates so that we don't issue a
  // ton of updates as a window is dragged around.
  if (MergeAdminTemplateWindowUpdate(*admin_template_, update)) {
    template_update_cb_.Run(*admin_template_);
  }
}

}  // namespace ash
