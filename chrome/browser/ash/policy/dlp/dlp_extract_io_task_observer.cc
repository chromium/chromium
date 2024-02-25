// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_extract_io_task_observer.h"

#include "base/files/file_enumerator.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "content/public/browser/browser_thread.h"

namespace policy {
namespace {

::dlp::AddFilesRequest ToAddFilesRequest(
    std::vector<base::FilePath> destination_directories,
    std::vector<std::string> dlp_sources) {
  CHECK_EQ(destination_directories.size(), dlp_sources.size());

  ::dlp::AddFilesRequest request;
  for (size_t index = 0; index < dlp_sources.size(); ++index) {
    if (dlp_sources[index].empty()) {
      // Non managed files do not have a source set.
      continue;
    }
    base::FileEnumerator filetraversal(destination_directories[index],
                                       /*recursive=*/true,
                                       base::FileEnumerator::FILES);
    for (base::FilePath name = filetraversal.Next(); !name.empty();
         name = filetraversal.Next()) {
      ::dlp::AddFileRequest* file_request = request.add_add_file_requests();
      file_request->set_file_path(name.value());
      file_request->set_source_url(dlp_sources[index]);
    }
  }
  return request;
}

void GotDlpMetadata(
    std::vector<base::FilePath> destination_directories,
    std::vector<DlpFilesControllerAsh::DlpFileMetadata> dlp_metadata_list) {
  if (dlp_metadata_list.empty() || !chromeos::DlpClient::Get() ||
      !chromeos::DlpClient::Get()->IsAlive()) {
    return;
  }

  std::vector<std::string> dlp_sources;
  dlp_sources.reserve(dlp_metadata_list.size());
  for (const auto& metadata : dlp_metadata_list) {
    dlp_sources.emplace_back(metadata.source_url);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ToAddFilesRequest, std::move(destination_directories),
                     std::move(dlp_sources)),
      base::BindOnce([](::dlp::AddFilesRequest request) {
        if (chromeos::DlpClient::Get() &&
            chromeos::DlpClient::Get()->IsAlive()) {
          chromeos::DlpClient::Get()->AddFiles(request, base::DoNothing());
        }
      }));
}

std::vector<storage::FileSystemURL> CollectSourceUrls(
    const std::vector<file_manager::io_task::EntryStatus>& entries) {
  std::vector<storage::FileSystemURL> archive_urls;
  archive_urls.reserve(entries.size());
  for (const auto& entry : entries) {
    CHECK(entry.source_url.has_value());
    archive_urls.push_back(entry.source_url.value());
  }
  return archive_urls;
}

std::vector<base::FilePath> CollectUrlPaths(
    const std::vector<file_manager::io_task::EntryStatus>& entries) {
  std::vector<base::FilePath> destination_directories;
  destination_directories.reserve(entries.size());
  for (const auto& entry : entries) {
    destination_directories.push_back(entry.url.path());
  }
  return destination_directories;
}

}  // namespace

DlpExtractIOTaskObserver::DlpExtractIOTaskObserver(
    file_manager::io_task::IOTaskController& io_task_controller) {
  io_task_controller_observation_.Observe(&io_task_controller);
}

DlpExtractIOTaskObserver::~DlpExtractIOTaskObserver() = default;

void DlpExtractIOTaskObserver::OnIOTaskStatus(
    const file_manager::io_task::ProgressStatus& status) {
  if (status.type != file_manager::io_task::OperationType::kExtract ||
      !status.IsCompleted()) {
    return;
  }

  policy::DlpFilesControllerAsh* files_controller =
      policy::DlpFilesControllerAsh::GetForPrimaryProfile();
  if (!files_controller) {
    return;
  }

  files_controller->GetDlpMetadata(
      CollectSourceUrls(status.outputs), /*destination=*/std::nullopt,
      base::BindOnce(&GotDlpMetadata, CollectUrlPaths(status.outputs)));
}

}  // namespace policy
