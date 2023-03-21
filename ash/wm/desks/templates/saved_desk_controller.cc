// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_controller.h"

namespace ash {

SavedDeskController::SavedDeskController() = default;

SavedDeskController::~SavedDeskController() = default;

std::vector<AdminTemplateMetadata>
SavedDeskController::GetAdminTemplateMetadata() const {
  return {};
}

bool SavedDeskController::LaunchAdminTemplate(const base::GUID& template_uuid) {
  return false;
}

}  // namespace ash
