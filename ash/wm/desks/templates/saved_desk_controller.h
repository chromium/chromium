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

 private:
  friend class SavedDeskControllerTestApi;

  // On success returns AdminTemplateBackend interface.  On failure returns
  // nullptr.
  desks_storage::AdminTemplateModel* GetAdminModel() const;

  // Invoked when the user has interacted with windows from a launched template.
  void OnAdminTemplateUpdate(const DeskTemplate& admin_template);

  std::unique_ptr<DeskTemplate> GetAdminTemplate(
      const base::Uuid& template_uuid) const;

  // Install an admin template that can be used by `LaunchAdminTemplate`.
  void SetAdminTemplateForTesting(std::unique_ptr<DeskTemplate> admin_template);

  base::flat_map<base::Uuid, std::unique_ptr<AdminTemplateLaunchTracker>>
      admin_template_launch_trackers_;

  // An optional admin template used for testing.
  std::unique_ptr<DeskTemplate> admin_template_for_testing_;
};

}  // namespace ash

#endif
