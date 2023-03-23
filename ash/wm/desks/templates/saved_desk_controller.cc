// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_controller.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/json/json_string_value_serializer.h"
#include "base/time/time.h"
#include "base/values.h"
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

}  // namespace

SavedDeskController::SavedDeskController() = default;

SavedDeskController::~SavedDeskController() = default;

std::vector<AdminTemplateMetadata>
SavedDeskController::GetAdminTemplateMetadata() const {
  return {AdminTemplateMetadata{
      .uuid = base::GUID::ParseLowercase(kPlaceholderUuid),
      .name = kPlaceholderName}};
}

bool SavedDeskController::LaunchAdminTemplate(const base::GUID& template_uuid) {
  auto placeholder_template = CreatePlaceholderTemplate();
  if (!placeholder_template || template_uuid != placeholder_template->uuid()) {
    return false;
  }

  // Set apps to launch on the current desk.
  auto* desks_controller = DesksController::Get();
  const int desk_index =
      desks_controller->GetDeskIndex(desks_controller->active_desk());
  placeholder_template->SetDeskIndex(desk_index);

  Shell::Get()->saved_desk_delegate()->LaunchAppsFromSavedDesk(
      std::move(placeholder_template));

  return true;
}

}  // namespace ash
