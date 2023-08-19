// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_CONTROLLER_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "components/desks_storage/core/admin_template_model.h"

namespace ash {

class AdminTemplateLaunchTracker;
class DeskTemplate;

struct AdminTemplateMetadata {
  // Uniquely identifies the template.
  base::Uuid uuid;

  // Name of the admin template, as it appears to the user.
  std::u16string name;
};

// The saved desk controller has functionality for listing and launching saved
// desks. Primarily geared towards admin templates. It is owned by ash::Shell.
class ASH_EXPORT SavedDeskController {
 public:
  SavedDeskController();
  SavedDeskController(const SavedDeskController&) = delete;
  SavedDeskController& operator=(const SavedDeskController&) = delete;
  virtual ~SavedDeskController();

  static SavedDeskController* Get();

  // Returns metadata for all currently available admin templates.
  virtual std::vector<AdminTemplateMetadata> GetAdminTemplateMetadata() const;

  // Launch the template identified by `template_uuid`. Returns false if the
  // template doesn't exist. By default, windows will open on the display
  // identified by `default_display_id`.
  virtual bool LaunchAdminTemplate(const base::Uuid& template_uuid,
                                   int64_t default_display_id);

  // This will initiate automatic launching of an admin template. If there are
  // no admin templates available, or none have been marked for auto launch,
  // then nothing will happen. The actual launching will happen at some near
  // point in the future. The passed `done` callback is invoked when auto launch
  // is done.
  virtual void InitiateAdminTemplateAutoLaunch(base::OnceCallback<void()> done);

 private:
  friend class SavedDeskControllerTestApi;

  // Represents data needed to support an admin template auto launch.
  struct AdminTemplateAutoLaunch {
    AdminTemplateAutoLaunch();
    ~AdminTemplateAutoLaunch();

    // Invoked when the launch has completed.
    base::OnceCallback<void()> done_callback;

    // Used when waiting for conditions to be right for an auto template launch.
    base::OneShotTimer launch_timer;

    // Tracks the duration since auto launch was initiated.
    base::ElapsedTimer elapsed_since_initiation;
  };

  // On success returns AdminTemplateBackend interface. If there are any errors
  // (for example, the model isn't yet ready), nullptr is returned.
  desks_storage::AdminTemplateModel* GetAdminModel() const;

  // Launches the template specified in `admin_template` (which cannot be null).
  void LaunchAdminTemplateImpl(std::unique_ptr<DeskTemplate> admin_template,
                               int64_t default_display_id);

  // Invoked when the user has interacted with windows from a launched template.
  void OnAdminTemplateUpdate(const DeskTemplate& admin_template);

  void AttemptAdminTemplateAutoLaunch();

  std::unique_ptr<DeskTemplate> GetAdminTemplate(
      const base::Uuid& template_uuid) const;

  // Removes all inactive admin template trackers. These are trackers that are
  // no longer tracking any windows.
  void RemoveInactiveAdminTemplateTrackers();

  // Install an admin template that can be used by `LaunchAdminTemplate`.
  void SetAdminTemplateForTesting(std::unique_ptr<DeskTemplate> admin_template);

  // Resets an in-progress auto launch, so that another can be run.
  void ResetAutoLaunchForTesting();

  base::flat_map<base::Uuid, std::unique_ptr<AdminTemplateLaunchTracker>>
      admin_template_launch_trackers_;

  // This is populated when an admin template auto launch is in progress.
  std::unique_ptr<AdminTemplateAutoLaunch> admin_template_auto_launch_;

  // An optional admin template used for testing.
  std::unique_ptr<DeskTemplate> admin_template_for_testing_;

  base::WeakPtrFactory<SavedDeskController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif
