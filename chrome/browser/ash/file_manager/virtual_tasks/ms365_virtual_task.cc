// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/virtual_tasks/ms365_virtual_task.h"

#include <string>
#include <vector>

#include "ash/webui/file_manager/url_constants.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "url/gurl.h"

namespace file_manager::file_tasks {

Ms365VirtualTask::Ms365VirtualTask() {
  for (const auto& extension : WordGroupExtensions()) {
    matcher_file_extensions_.push_back(extension);
  }
  for (const auto& extension : ExcelGroupExtensions()) {
    matcher_file_extensions_.push_back(extension);
  }
  for (const auto& extension : PowerPointGroupExtensions()) {
    matcher_file_extensions_.push_back(extension);
  }

  for (const auto& mime_type : WordGroupMimeTypes()) {
    matcher_mime_types_.push_back(mime_type);
  }
  for (const auto& mime_type : ExcelGroupMimeTypes()) {
    matcher_mime_types_.push_back(mime_type);
  }
  for (const auto& mime_type : PowerPointGroupMimeTypes()) {
    matcher_mime_types_.push_back(mime_type);
  }
}

bool Ms365VirtualTask::IsEnabled(Profile* profile) const {
  return chromeos::cloud_upload::IsMicrosoftOfficeCloudUploadAllowed(profile) &&
         (!profile->GetProfilePolicyConnector()->IsManaged() ||
          ash::cloud_upload::IsODFSInstalled(profile));
}

std::string Ms365VirtualTask::id() const {
  return base::StrCat(
      {ash::file_manager::kChromeUIFileManagerURL, "?", kActionIdOpenInOffice});
}

std::string Ms365VirtualTask::title() const {
  return l10n_util::GetStringUTF8(IDS_FILE_BROWSER_TASK_OPEN_MICROSOFT_365);
}

GURL Ms365VirtualTask::icon_url() const {
  // This gets overridden in file_tasks.ts.
  // TODO(crbug.com/40280769): Specify the icon here instead of overriding it.
  return GURL();
}

bool Ms365VirtualTask::IsDlpBlocked(
    const std::vector<std::string>& dlp_source_urls) const {
  // A transfer to OneDrive is required for the Office PWA to open files, if
  // transferring files to OneDrive is restricted, we gray out the corresponding
  // task.
  return policy::dlp::IsFilesTransferBlocked(
      dlp_source_urls, data_controls::Component::kOneDrive);
}

bool Ms365VirtualTask::Execute(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_urls) const {
  base::UmaHistogramSparse(
      ash::cloud_upload::kNumberOfFilesToOpenWithOneDriveMetric,
      file_urls.size());
  // Only attempt to open the first selected file, as a temporary way to
  // avoid conflicts and error inconsistencies.
  // TODO(b/242685536) add support for multiple files.
  FileSystemURL file_url = file_urls[0];
  RecordOfficeOpenExtensionOneDriveMetric(file_url);
  return ExecuteOpenInOfficeTask(
      profile, task, {file_url},
      std::make_unique<ash::cloud_upload::CloudOpenMetrics>(
          ash::cloud_upload::CloudProvider::kOneDrive, 1));
}

}  // namespace file_manager::file_tasks
