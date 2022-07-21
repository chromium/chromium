// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/office_task_selection_helper.h"
#include "ash/constants/ash_features.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"

namespace file_manager::file_tasks {

OfficeTaskSelectionHelper::OfficeTaskSelectionHelper(
    Profile* profile,
    const std::vector<extensions::EntryInfo>& entries,
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
    std::set<std::string> disabled_actions)
    : profile(profile),
      entries(entries),
      result_list(std::move(result_list)),
      disabled_actions(std::move(disabled_actions)) {}

OfficeTaskSelectionHelper::~OfficeTaskSelectionHelper() = default;

void OfficeTaskSelectionHelper::Run(base::OnceClosure callback) {
  DCHECK(callback);
  DCHECK(!callback_);

  callback_ = std::move(callback);
  AdjustTasks();
}

bool OfficeTaskSelectionHelper::IsCandidateWebDriveOffice() {
  return candidate_office_action_id_ == kActionIdWebDriveOfficeWord ||
         candidate_office_action_id_ == kActionIdWebDriveOfficeExcel ||
         candidate_office_action_id_ == kActionIdWebDriveOfficePowerPoint;
}

bool OfficeTaskSelectionHelper::IsCandidateUploadOfficeToDrive() {
  return candidate_office_action_id_ == kActionIdUploadOfficeToDrive;
}

void OfficeTaskSelectionHelper::InvalidateCandidate() {
  candidate_office_action_id_ = std::string();
}

std::string OfficeTaskSelectionHelper::ExtensionToWebDriveOfficeActionId(
    std::string extension) {
  if (extension == ".doc" || extension == ".docx") {
    return kActionIdWebDriveOfficeWord;
  }
  if (extension == ".xls" || extension == ".xlsx") {
    return kActionIdWebDriveOfficeExcel;
  }
  if (extension == ".ppt" || extension == ".pptx") {
    return kActionIdWebDriveOfficePowerPoint;
  }
  return std::string();
}

// Sets `candidate_office_action_id_` as the potential action ID that can
// handle the selected Office files. The candidate is the relevant "Web
// Drive Office" action ID if the entries are on Drive, "Upload to Drive" if
// the entries are outside Drive, or the empty string if no candidate can be
// set. Returns whether a candidate was found.
bool OfficeTaskSelectionHelper::SetCandidateActionId() {
  bool not_on_drive = false;
  for (const auto& entry : entries) {
    const std::string extension =
        base::FilePath(entry.path.Extension()).AsUTF8Unsafe();
    // Check whether the entry is on Drive.
    if (::file_manager::util::IsDriveLocalPath(profile, entry.path)) {
      // Candidate: Web Drive Office.
      std::string web_drive_office_action_id =
          ExtensionToWebDriveOfficeActionId(extension);
      if (!candidate_office_action_id_.empty() &&
          candidate_office_action_id_ != web_drive_office_action_id) {
        // The action IDs associated to the selected entries are conflicting.
        // Disable Office file handling.
        InvalidateCandidate();
        return false;
      }
      candidate_office_action_id_ = web_drive_office_action_id;
    } else {
      // Candidate: Upload to Drive.
      DCHECK(candidate_office_action_id_.empty() ||
             candidate_office_action_id_ == kActionIdUploadOfficeToDrive);
      candidate_office_action_id_ = kActionIdUploadOfficeToDrive;
      not_on_drive = true;
    }
  }
  if (not_on_drive) {
    // Record the "Not on Drive" Web Drive Office metric.
    UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                              WebDriveOfficeTaskResult::NOT_ON_DRIVE);
  }
  return !candidate_office_action_id_.empty();
}

// Starts processing entries to determine the Office task, if any, to enable.
void OfficeTaskSelectionHelper::AdjustTasks() {
  const auto handle_office_task = std::find_if(
      result_list->begin(), result_list->end(), &IsHandleOfficeTask);
  if (handle_office_task == result_list->end() || !SetCandidateActionId()) {
    EndAdjustTasks();
    return;
  }

  // If the Upload to Drive flag is disabled, invalidate Upload to Drive.
  if (!ash::features::IsUploadOfficeToCloudEnabled() &&
      IsCandidateUploadOfficeToDrive()) {
    InvalidateCandidate();
    EndAdjustTasks();
    return;
  }

  // If the Web Drive Office flag is disabled, invalidate Web Drive Office, and
  // also Upload to Drive which is dependent on Web Drive Office.
  if (!ash::features::IsFilesWebDriveOfficeEnabled()) {
    if (IsCandidateWebDriveOffice()) {
      UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                                WebDriveOfficeTaskResult::FLAG_DISABLED);
      InvalidateCandidate();
      EndAdjustTasks();
      return;
    }
    if (IsCandidateUploadOfficeToDrive()) {
      InvalidateCandidate();
      EndAdjustTasks();
      return;
    }
  }

  // Disable Office file handling if Drive is Offline.
  if (drive::util::GetDriveConnectionStatus(profile) !=
      drive::util::DRIVE_CONNECTED) {
    if (IsCandidateWebDriveOffice()) {
      UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                                WebDriveOfficeTaskResult::OFFLINE);
    }
    InvalidateCandidate();
    EndAdjustTasks();
    return;
  }

  // Disable Office file handling if the DriveIntegrationService is not
  // available.
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!(integration_service && integration_service->IsMounted() &&
        integration_service->GetDriveFsInterface())) {
    if (IsCandidateWebDriveOffice()) {
      UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                                WebDriveOfficeTaskResult::DRIVE_ERROR);
    }
    InvalidateCandidate();
    EndAdjustTasks();
    return;
  }

  if (IsCandidateWebDriveOffice()) {
    ProcessNextEntryForAlternateUrl(0);
    return;
  }

  EndAdjustTasks();
}

// Checks whether an entry is potentially available to be opened and edited in
// Web Drive, and query its DriveFS metadata for files on Drive.
void OfficeTaskSelectionHelper::ProcessNextEntryForAlternateUrl(
    size_t entry_index) {
  if (entry_index == entries.size()) {
    UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                              WebDriveOfficeTaskResult::AVAILABLE);
    EndAdjustTasks();
    return;
  }

  base::FilePath relative_drive_path;
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!integration_service->GetRelativeDrivePath(entries[entry_index].path,
                                                 &relative_drive_path)) {
    UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                              WebDriveOfficeTaskResult::DRIVE_ERROR);
    InvalidateCandidate();
    EndAdjustTasks();
    return;
  }

  // Get Office file's metadata.
  integration_service->GetDriveFsInterface()->GetMetadata(
      relative_drive_path,
      base::BindOnce(
          &OfficeTaskSelectionHelper::OnGetDriveFsMetadataForWebDriveOffice,
          weak_factory_.GetWeakPtr(), entry_index));
}

// Checks whether the Web Drive Office task should be disabled based on the
// entry's alternate URL.
void OfficeTaskSelectionHelper::OnGetDriveFsMetadataForWebDriveOffice(
    size_t entry_index,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK) {
    UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                              WebDriveOfficeTaskResult::DRIVE_METADATA_ERROR);
    InvalidateCandidate();
    EndAdjustTasks();
    return;
  }

  GURL hosted_url(metadata->alternate_url);
  // URLs for editing Office files in Web Drive all have a "docs.google.com"
  // host: Disable the task if the entry doesn't have such alternate URL.
  if (!hosted_url.is_valid()) {
    UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                              WebDriveOfficeTaskResult::INVALID_ALTERNATE_URL);
    InvalidateCandidate();
    EndAdjustTasks();
    return;
  } else if (hosted_url.host() == "drive.google.com") {
    UMA_HISTOGRAM_ENUMERATION(kWebDriveOfficeMetricName,
                              WebDriveOfficeTaskResult::DRIVE_ALTERNATE_URL);
    InvalidateCandidate();
    EndAdjustTasks();
    return;
  } else if (hosted_url.host() != "docs.google.com") {
    UMA_HISTOGRAM_ENUMERATION(
        kWebDriveOfficeMetricName,
        WebDriveOfficeTaskResult::UNEXPECTED_ALTERNATE_URL);
    InvalidateCandidate();
    EndAdjustTasks();
    return;
  }

  // Check alternate URL for next entry.
  ProcessNextEntryForAlternateUrl(++entry_index);
}

// Ends the recursion that determines whether or not the Web Drive Office
// action is available.
void OfficeTaskSelectionHelper::EndAdjustTasks() {
  if (candidate_office_action_id_.empty()) {
    disabled_actions.emplace(kActionIdHandleOffice);
  } else {
    // The action ID to use for the selected Office files has been found.
    // Replace the generic "handle-office" action ID with
    // `candidate_office_action_id_`.
    const auto handle_office_task = std::find_if(
        result_list->begin(), result_list->end(), &IsHandleOfficeTask);
    DCHECK(handle_office_task != result_list->end());
    std::string prefix =
        ash::features::IsFileManagerSwaEnabled()
            ? std::string(ash::file_manager::kChromeUIFileManagerURL) + "?"
            : "";
    handle_office_task->task_descriptor.action_id =
        prefix + candidate_office_action_id_;
  }
  std::move(callback_).Run();
}

}  // namespace file_manager::file_tasks
