// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_controller.h"

#include <map>

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/session/session_controller_impl.h"
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

  for (const ash::DeskTemplate* entry : entries_lookup_result.entries) {
    out_metadata->push_back(AdminTemplateMetadata{
        .uuid = entry->uuid(), .name = entry->template_name()});
  }
}

// A simple exponential back-off with a max cap.
base::TimeDelta GetModelWaitDuration(base::TimeDelta last_wait_duration) {
  return std::min(base::Seconds(1), last_wait_duration * 2);
}

// Returns true if `saved_desk` contains no windows.
bool IsEmptySavedDesk(const DeskTemplate& saved_desk) {
  if (auto* restore_data = saved_desk.desk_restore_data()) {
    for (const auto& [app_id, launch_list] :
         restore_data->app_id_to_launch_list()) {
      if (!launch_list.empty()) {
        return false;
      }
    }
  }

  return true;
}

// Pointer to the global `SavedDeskController` instance.
SavedDeskController* g_instance = nullptr;

}  // namespace

SavedDeskController::AdminTemplateAutoLaunch::AdminTemplateAutoLaunch() =
    default;

SavedDeskController::AdminTemplateAutoLaunch::~AdminTemplateAutoLaunch() =
    default;

bool ScrubLacrosProfileFromSavedDesk(DeskTemplate& saved_desk,
                                     uint64_t lacros_profile_id,
                                     uint64_t primary_user_lacros_profile_id) {
  // This function should not be called with a default Lacros profile ID.
  CHECK(lacros_profile_id);

  bool modified = false;

  // Update the desk association, if necessary.
  if (saved_desk.lacros_profile_id() == lacros_profile_id) {
    saved_desk.set_lacros_profile_id(primary_user_lacros_profile_id);
    modified = true;
  }

  auto* restore_data = saved_desk.mutable_desk_restore_data();
  if (!restore_data) {
    return modified;
  }

  auto& app_id_to_launch_list = restore_data->mutable_app_id_to_launch_list();

  // The restore data model is a two-level tree. The first level maps app IDs to
  // launch lists. A launch list in turn maps a window id to data for that
  // window. Here we traverse the entire tree. We find and erase any window with
  // a matching lacros ID. If this results in an empty launch list, then that
  // list is erased from the root.
  std::erase_if(app_id_to_launch_list, [&](auto& launch_list_entry) {
    auto& launch_list = launch_list_entry.second;

    std::erase_if(launch_list, [&](auto& window_entry) {
      auto& app_restore_data = window_entry.second;
      if (app_restore_data->browser_extra_info.lacros_profile_id !=
          lacros_profile_id) {
        return false;
      }

      // Erase the window entry if the lacros profile id matches.
      modified = true;
      return true;
    });

    // Erase the launch list entry if the launch list is empty. This also cleans
    // things up if the launch list happened to be empty to begin with.
    if (launch_list.empty()) {
      modified = true;
      return true;
    }
    return false;
  });

  return modified;
}

// SavedDeskController
SavedDeskController::SavedDeskController() {
  CHECK(!g_instance);
  g_instance = this;

  // We want to observe DeskProfilesDelegate, but it won't be available until
  // later. We first register as a session observer so that we can detect when
  // the first session has started (which also means the DeskProfilesDelegate
  // will be available).
  if (Shell::HasInstance()) {
    // Note that the shell is not available in some unit tests.
    session_observer_.Observe(Shell::Get()->session_controller());
  }
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
    for (const ash::DeskTemplate* admin_template : result.entries) {
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

void SavedDeskController::OnFirstSessionStarted() {
  // The DeskProfilesDelegate will be available if lacros and desk profiles are
  // both enabled.
  if (auto* delegate = Shell::Get()->GetDeskProfilesDelegate()) {
    desk_profiles_observer_.Observe(delegate);
  }

  // We no longer need to observe the session updates.
  session_observer_.Reset();
}

void SavedDeskController::OnProfileRemoved(uint64_t profile_id) {
  desks_storage::DeskModel* model =
      Shell::Get()->saved_desk_delegate()->GetDeskModel();

  if (!model) {
    return;
  }

  uint64_t primary_user_profile_id = 0;
  if (auto* delegate = Shell::Get()->GetDeskProfilesDelegate()) {
    primary_user_profile_id = delegate->GetPrimaryProfileId();
  }

  // Get all the entries in the model. For each entry, scrub data that belongs
  // to the deleted profile. Modifications are written back to the model.
  for (const ash::DeskTemplate* saved_desk_from_model :
       model->GetAllEntries().entries) {
    auto saved_desk = saved_desk_from_model->Clone();
    if (ScrubLacrosProfileFromSavedDesk(*saved_desk, profile_id,
                                        primary_user_profile_id)) {
      // The saved desk has been updated. If it is now empty (no windows
      // remain), then we are going to delete it completely from the
      // model. Otherwise, we write the updated desk back to the model.
      if (IsEmptySavedDesk(*saved_desk)) {
        model->DeleteEntry(saved_desk->uuid(), base::DoNothing());
      } else {
        model->AddOrUpdateEntry(std::move(saved_desk), base::DoNothing());
      }
    }
  }
}

desks_storage::AdminTemplateModel* SavedDeskController::GetAdminModel() const {
  auto* admin_template_service =
      Shell::Get()->saved_desk_delegate()->GetAdminTemplateService();

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
