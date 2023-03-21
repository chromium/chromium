// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_CONTROLLER_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/guid.h"

namespace ash {

struct AdminTemplateMetadata {
  // Uniquely identifies the template.
  base::GUID uuid;

  // Name of the admin template, as it appears to the user.
  std::string name;
};

// The saved desk controller has functionality for listing and launching saved
// desks. Primarily geared towards admin templates. It is owned by ash::Shell.
class ASH_EXPORT SavedDeskController {
 public:
  SavedDeskController();
  SavedDeskController(const SavedDeskController&) = delete;
  SavedDeskController& operator=(const SavedDeskController&) = delete;
  ~SavedDeskController();

  // Returns metadata for all currently available admin templates.
  std::vector<AdminTemplateMetadata> GetAdminTemplateMetadata() const;

  // Launch the template identified by `template_uuid`. Returns false if the
  // template doesn't exist.
  bool LaunchAdminTemplate(const base::GUID& template_uuid);
};

}  // namespace ash

#endif
