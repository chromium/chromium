// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_controller.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/shell.h"
#include "ash/wm/desks/templates/admin_template_launch_tracker.h"
#include "base/check.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/utf_string_conversions.h"
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
      base::Uuid::ParseLowercase(kPlaceholderUuid), DeskTemplateSource::kPolicy,
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
      .uuid = base::Uuid::ParseLowercase(kPlaceholderUuid),
      .name = base::UTF8ToUTF16(base::StringPiece(kPlaceholderName))}};
}

bool SavedDeskController::LaunchAdminTemplate(const base::Uuid& template_uuid,
                                              int64_t default_display_id) {
  auto admin_template = GetAdminTemplate(template_uuid);
  if (!admin_template) {
    return false;
  }

  int64_t launch_id = ++admin_template_launch_id_;
  admin_template_launch_trackers_[launch_id] =
      std::make_unique<AdminTemplateLaunchTracker>(
          std::move(admin_template),
          base::BindRepeating(&SavedDeskController::OnAdminTemplateUpdate,
                              base::Unretained(this)));

  admin_template_launch_trackers_[launch_id]->LaunchTemplate(
      Shell::Get()->saved_desk_delegate(), default_display_id);

  // TODO(dandersson): Remove the launch tracker when all its windows have been
  // closed.

  return true;
}

void SavedDeskController::OnAdminTemplateUpdate(
    const DeskTemplate& admin_template) {
  // TODO(dandersson): Write to desk model.
}

std::unique_ptr<DeskTemplate> SavedDeskController::GetAdminTemplate(
    const base::Uuid& template_uuid) const {
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
