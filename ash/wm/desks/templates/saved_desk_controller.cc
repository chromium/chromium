// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_controller.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/wm/desks/templates/admin_template_launch_tracker.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "base/time/time.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/restore_data.h"
#include "components/desks_storage/core/admin_template_model.h"
#include "components/desks_storage/core/admin_template_service.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash {

namespace {

// Updates to the admin template are throttled to this interval.
constexpr base::TimeDelta kAdminTemplateUpdateDelay = base::Seconds(5);

// If the admin storage model isn't available when an auto launch is initiated,
// we will keep retrying for this amount of time before giving up.
constexpr base::TimeDelta kMaxAutoLaunchAttempt = base::Seconds(10);

void PopulateAdminTemplateMetadata(
    const desks_storage::DeskModel::GetAllEntriesResult& entries_lookup_result,
    std::vector<AdminTemplateMetadata>* out_metadata) {
  // If something goes wrong, log it and exit.
  if (entries_lookup_result.status !=
      desks_storage::DeskModel::GetAllEntriesStatus::kOk) {
    LOG(WARNING) << "GetAllEntries returned "
                 << static_cast<int>(entries_lookup_result.status);
    return;
  }

  for (auto* entry : entries_lookup_result.entries) {
    out_metadata->push_back(AdminTemplateMetadata{
        .uuid = entry->uuid(), .name = entry->template_name()});
  }
}

// A simple exponential back-off with a max cap.
base::TimeDelta GetModelWaitDuration(base::TimeDelta last_wait_duration) {
  return std::min(base::Seconds(1), last_wait_duration * 2);
}

// Pointer to the global `SavedDeskController` instance.
SavedDeskController* g_instance = nullptr;

}  // namespace

SavedDeskController::AdminTemplateAutoLaunch::AdminTemplateAutoLaunch() =
    default;

SavedDeskController::AdminTemplateAutoLaunch::~AdminTemplateAutoLaunch() =
    default;

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

  return metadata;
}

bool SavedDeskController::LaunchAdminTemplate(const base::Uuid& template_uuid,
                                              int64_t default_display_id) {
  // Check if we are currently tracking a previous launched instance of this
  // template. If so, we will flush any pending updates and then destroy the
  // tracker.  This will ensure that we get the most recent version when
  // querying the admin model.
  auto it = admin_template_launch_trackers_.find(template_uuid);
  if (it != admin_template_launch_trackers_.end()) {
    it->second->FlushPendingUpdate();
    admin_template_launch_trackers_.erase(it);
  }

  // This will remove any admin template trackers that are no longer tracking
  // any windows. Note that for the common case of only having a single admin
  // template, the previous operation will have already removed the tracker.
  RemoveInactiveAdminTemplateTrackers();

  auto admin_template = GetAdminTemplate(template_uuid);
  if (!admin_template) {
    return false;
  }

  RecordAdminTemplateWindowAndTabCountHistogram(*admin_template);
  RecordLaunchAdminTemplateHistogram();

  LaunchAdminTemplateImpl(std::move(admin_template), default_display_id);
  return true;
}

void SavedDeskController::InitiateAdminTemplateAutoLaunch(
    base::OnceCallback<void()> done) {
  // We do not allow concurrent auto launch requests.
  if (admin_template_auto_launch_) {
    return;
  }

  admin_template_auto_launch_ = std::make_unique<AdminTemplateAutoLaunch>();
  admin_template_auto_launch_->done_callback = std::move(done);
  admin_template_auto_launch_->launch_timer.Start(
      FROM_HERE, base::Milliseconds(50),
      base::BindOnce(&SavedDeskController::AttemptAdminTemplateAutoLaunch,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedDeskController::OnAdminTemplateUpdate(
    const DeskTemplate& admin_template) {
  if (auto* admin_model = GetAdminModel()) {
    admin_model->UpdateEntry(admin_template.Clone());
  }
}

void SavedDeskController::AttemptAdminTemplateAutoLaunch() {
  if (!admin_template_auto_launch_) {
    return;
  }

  auto* admin_model = GetAdminModel();
  if (!admin_model) {
    if (admin_template_auto_launch_->elapsed_since_initiation.Elapsed() >
        kMaxAutoLaunchAttempt) {
      return;
    }
    admin_template_auto_launch_->launch_timer.Start(
        FROM_HERE,
        GetModelWaitDuration(
            admin_template_auto_launch_->launch_timer.GetCurrentDelay()),
        base::BindOnce(&SavedDeskController::AttemptAdminTemplateAutoLaunch,
                       weak_ptr_factory_.GetWeakPtr()));

    return;
  }

  // Moves to a local pointer so that the auto launch data is destroyed at the
  // end of this function.
  auto auto_launch = std::move(admin_template_auto_launch_);

  // The model is now ready. We'll now retrieve all entries and launch the ones
  // that are marked for launching on startup.
  auto result = admin_model->GetAllEntries();
  if (result.status == desks_storage::DeskModel::GetAllEntriesStatus::kOk) {
    for (const auto* admin_template : result.entries) {
      if (admin_template->should_launch_on_startup()) {
        LaunchAdminTemplateImpl(
            admin_template->Clone(),
            display::Screen::GetScreen()->GetPrimaryDisplay().id());
      }
    }
  }

  if (auto_launch->done_callback) {
    std::move(auto_launch->done_callback).Run();
  }
}

desks_storage::AdminTemplateModel* SavedDeskController::GetAdminModel() const {
  auto* admin_template_service =
      ash::Shell::Get()->saved_desk_delegate()->GetAdminTemplateService();

  if (!admin_template_service || !admin_template_service->IsReady()) {
    return nullptr;
  }

  return admin_template_service->GetAdminModel();
}

void SavedDeskController::LaunchAdminTemplateImpl(
    std::unique_ptr<DeskTemplate> admin_template,
    int64_t default_display_id) {
  auto& tracker = admin_template_launch_trackers_[admin_template->uuid()];
  // Note: if there is an existing launch tracker for this template, this will
  // implicitly destroy it - no more updates will be received from the previous
  // instance.
  tracker = std::make_unique<AdminTemplateLaunchTracker>(
      std::move(admin_template),
      base::BindRepeating(&SavedDeskController::OnAdminTemplateUpdate,
                          weak_ptr_factory_.GetWeakPtr()),
      kAdminTemplateUpdateDelay);
  tracker->LaunchTemplate(Shell::Get()->saved_desk_delegate(),
                          default_display_id);
}

std::unique_ptr<DeskTemplate> SavedDeskController::GetAdminTemplate(
    const base::Uuid& template_uuid) const {
  if (admin_template_for_testing_ &&
      admin_template_for_testing_->uuid() == template_uuid) {
    return admin_template_for_testing_->Clone();
  }

  if (auto* admin_model = GetAdminModel()) {
    auto result = admin_model->GetEntryByUUID(template_uuid);

    if (result.status != desks_storage::DeskModel::GetEntryByUuidStatus::kOk) {
      LOG(WARNING) << "GetEntryByUUID returned "
                   << static_cast<int>(result.status);
      return nullptr;
    }

    return std::move(result.entry);
  }

  // Failed to get model, return nullptr.
  return nullptr;
}

void SavedDeskController::RemoveInactiveAdminTemplateTrackers() {
  for (auto it = admin_template_launch_trackers_.begin();
       it != admin_template_launch_trackers_.end();) {
    if (!it->second->IsActive()) {
      it->second->FlushPendingUpdate();
      it = admin_template_launch_trackers_.erase(it);
    } else {
      ++it;
    }
  }
}

void SavedDeskController::SetAdminTemplateForTesting(
    std::unique_ptr<DeskTemplate> admin_template) {
  admin_template_for_testing_ = std::move(admin_template);
}

void SavedDeskController::ResetAutoLaunchForTesting() {
  admin_template_auto_launch_.reset();
}

}  // namespace ash
