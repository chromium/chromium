// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_controller.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/templates/saved_desk_constants.h"
#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/json/json_string_value_serializer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/restore_data.h"

namespace ash {

namespace {

constexpr char kPlaceholderUuid[] = "2a0fe322-c912-468e-bd9c-5e8fddcc1606";
constexpr char kPlaceholderName[] = "Test template";
constexpr char kPlaceholderJson[] = R"json(
{
   "mgndgikekgjfcpckkfioiadnlibdjbkf": {
      "1": {
         "active_tab_index": 0,
         "app_name": "",
         "current_bounds": [ 100, 50, 640, 480 ],
         "display_id": "2200000000",
         "index": 0,
         "title": "Chrome",
         "urls": [ "https://www.google.com/" ],
         "window_state_type": 0
      }
   }
})json";

// Creates a placeholder template that will be used during development.
std::unique_ptr<DeskTemplate> CreatePlaceholderTemplate() {
  auto desk_template = std::make_unique<DeskTemplate>(
      base::GUID::ParseLowercase(kPlaceholderUuid), DeskTemplateSource::kPolicy,
      kPlaceholderName, base::Time::Now(), DeskTemplateType::kTemplate);

  // Create restore data from json.
  base::JSONReader::Result restore_data =
      base::JSONReader::ReadAndReturnValueWithError(kPlaceholderJson);
  if (!restore_data.has_value()) {
    return nullptr;
  }

  desk_template->set_desk_restore_data(
      std::make_unique<app_restore::RestoreData>(
          std::move(restore_data).value()));

  return desk_template;
}

// Pointer to the global `SavedDeskController` instance.
SavedDeskController* g_instance = nullptr;

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

}  // namespace

SavedDeskController::SavedDeskController() {
  CHECK(!g_instance);
  g_instance = this;
}

SavedDeskController::~SavedDeskController() {
  g_instance = nullptr;
}

SavedDeskController* SavedDeskController::Get() {
  return g_instance;
}

std::vector<AdminTemplateMetadata>
SavedDeskController::GetAdminTemplateMetadata() const {
  return {AdminTemplateMetadata{
      .uuid = base::GUID::ParseLowercase(kPlaceholderUuid),
      .name = base::UTF8ToUTF16(base::StringPiece(kPlaceholderName))}};
}

bool SavedDeskController::LaunchAdminTemplate(const base::GUID& template_uuid,
                                              int64_t default_display_id) {
  auto admin_template = GetAdminTemplate(template_uuid);
  if (!admin_template) {
    return false;
  }

  // Set apps to launch on the current desk.
  auto* desks_controller = DesksController::Get();
  const int desk_index =
      desks_controller->GetDeskIndex(desks_controller->active_desk());
  admin_template->SetDeskIndex(desk_index);

  UpdateAdminTemplateActivationIndices(*admin_template);

  Shell::Get()->saved_desk_delegate()->LaunchAppsFromSavedDesk(
      std::move(admin_template));

  return true;
}

std::unique_ptr<DeskTemplate> SavedDeskController::GetAdminTemplate(
    const base::GUID& template_uuid) const {
  if (admin_template_for_testing_ &&
      admin_template_for_testing_->uuid() == template_uuid) {
    return admin_template_for_testing_->Clone();
  }

  auto placeholder_template = CreatePlaceholderTemplate();
  if (placeholder_template && template_uuid == placeholder_template->uuid()) {
    return placeholder_template;
  }

  return nullptr;
}

void SavedDeskController::SetAdminTemplateForTesting(
    std::unique_ptr<DeskTemplate> admin_template) {
  admin_template_for_testing_ = std::move(admin_template);
}

}  // namespace ash
