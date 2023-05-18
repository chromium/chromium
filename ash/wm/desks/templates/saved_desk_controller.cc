// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_controller.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/wm/desks/templates/admin_template_launch_tracker.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/restore_data.h"
#include "components/desks_storage/core/admin_template_model.h"
#include "components/desks_storage/core/admin_template_service.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash {

namespace {

constexpr base::TimeDelta kAdminTemplateUpdateDelay = base::Seconds(5);

constexpr char kPlaceholderUuid[] = "2a0fe322-c912-468e-bd9c-5e8fddcc1606";
constexpr char kPlaceholderName[] = "Test template";
constexpr char kPlaceholderJson[] = R"json(
{
   "mgndgikekgjfcpckkfioiadnlibdjbkf": {
      "1": {
         "active_tab_index": 0,
         "app_name": "",
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

void PopulateAdminTemplateMetadata(
    const desks_storage::DeskModel::GetAllEntriesResult& entries_lookup_result,
    std::vector<AdminTemplateMetadata>* out_metadata) {
  // If something goes wrong, log it and exit.
  if (entries_lookup_result.status !=
      desks_storage::DeskModel::GetAllEntriesStatus::kOk) {
    LOG(WARNING) << "Get all entries did not return OK status!";
    return;
  }

  for (auto* entry : entries_lookup_result.entries) {
    out_metadata->push_back(AdminTemplateMetadata{
        .uuid = entry->uuid(), .name = entry->template_name()});
  }
}

// Pointer to the global `SavedDeskController` instance.
SavedDeskController* g_instance = nullptr;

}  // namespace

// SavedDeskController
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
  std::vector<AdminTemplateMetadata> metadata;

  if (auto* admin_model = GetAdminModel()) {
    PopulateAdminTemplateMetadata(admin_model->GetAllEntries(), &metadata);
  }

  // Make sure we always at least have the placeholder.
  metadata.push_back(AdminTemplateMetadata{
      .uuid = base::Uuid::ParseLowercase(kPlaceholderUuid),
      .name = base::UTF8ToUTF16(base::StringPiece(kPlaceholderName))});

  return metadata;
}

bool SavedDeskController::LaunchAdminTemplate(const base::Uuid& template_uuid,
                                              int64_t default_display_id) {
  auto admin_template = GetAdminTemplate(template_uuid);
  if (!admin_template) {
    return false;
  }

  RecordAdminTemplateWindowAndTabCountHistogram(*admin_template);

  auto& tracker = admin_template_launch_trackers_[template_uuid];
  // Note: if there is an existing launch tracker for this template, this will
  // implicitly destroy it - no more updates will be received from the previous
  // instance.
  tracker = std::make_unique<AdminTemplateLaunchTracker>(
      std::move(admin_template),
      base::BindRepeating(&SavedDeskController::OnAdminTemplateUpdate,
                          base::Unretained(this)),
      kAdminTemplateUpdateDelay);
  tracker->LaunchTemplate(Shell::Get()->saved_desk_delegate(),
                          default_display_id);

  // TODO(dandersson): Remove the launch tracker when all its windows have been
  // closed.

  RecordLaunchAdminTemplateHistogram();
  return true;
}

void SavedDeskController::OnAdminTemplateUpdate(
    const DeskTemplate& admin_template) {
  if (auto* admin_model = GetAdminModel()) {
    admin_model->UpdateEntry(admin_template.Clone());
  }
}

desks_storage::AdminTemplateModel* SavedDeskController::GetAdminModel() const {
  auto* admin_template_service =
      ash::Shell::Get()->saved_desk_delegate()->GetAdminTemplateService();

  return admin_template_service->GetAdminModel();
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

  if (auto* admin_model = GetAdminModel()) {
    auto result = admin_model->GetEntryByUUID(template_uuid);

    if (result.status != desks_storage::DeskModel::GetEntryByUuidStatus::kOk) {
      LOG(WARNING) << "Entry lookup failure!";
      return nullptr;
    }

    return std::move(result.entry);
  }

  // Failed to get model, return nullptr.
  return nullptr;
}

void SavedDeskController::SetAdminTemplateForTesting(
    std::unique_ptr<DeskTemplate> admin_template) {
  admin_template_for_testing_ = std::move(admin_template);
}

}  // namespace ash
