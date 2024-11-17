// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/virtual_file_tasks.h"

#include <initializer_list>
#include <vector>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/virtual_tasks/drive_upload_virtual_task.h"
#include "chrome/browser/ash/file_manager/virtual_tasks/install_isolated_web_app_virtual_task.h"
#include "chrome/browser/ash/file_manager/virtual_tasks/ms365_virtual_task.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "components/services/app_service/public/cpp/intent_util.h"

namespace file_manager::file_tasks {

namespace {

// The set of virtual tasks is statically determined. Tasks can turn themselves
// on or off dynamically by implementing |IsEnabled()|.
const std::vector<VirtualTask*>& GetVirtualTasks() {
  static const base::NoDestructor<std::vector<VirtualTask*>> virtual_tasks(
      std::initializer_list<VirtualTask*>({
          new InstallIsolatedWebAppVirtualTask(),
          new Ms365VirtualTask(),
          new DocsUploadVirtualTask(),
          new SheetsUploadVirtualTask(),
          new SlidesUploadVirtualTask(),
      }));
  if (!GetTestVirtualTasks().empty()) {
    return GetTestVirtualTasks();
  }
  return *virtual_tasks;
}

bool LooksLikeVirtualTask(const TaskDescriptor& task) {
  return task.app_id == kFileManagerSwaAppId &&
         task.task_type == TASK_TYPE_WEB_APP;
}

// Validates that each entry from `entries` matches any mime type from
// `mime_types`.
bool AllEntriesMatchAtLeastOneMimeType(
    const std::vector<extensions::EntryInfo>& entries,
    const std::vector<std::string>& mime_types) {
  return base::ranges::all_of(
      entries,
      [&](const std::string& entry_mime_type) {
        return base::ranges::any_of(
            mime_types, [&](const std::string& mime_type) {
              return apps_util::MimeTypeMatched(entry_mime_type, mime_type);
            });
      },
      &extensions::EntryInfo::mime_type);
}

// Validates that each file url from `file_urls` matches any file extension from
// `file_extensions`
bool AllUrlsMatchAtLeastOneFileExtension(
    const std::vector<GURL>& file_urls,
    const std::vector<std::string>& file_extensions) {
  return base::ranges::all_of(
      file_urls,
      [&](const std::string& file_name) {
        return base::ranges::any_of(
            file_extensions, [&](const std::string& file_extension) {
              return apps_util::ExtensionMatched(file_name, file_extension);
            });
      },
      &GURL::ExtractFileName);
}

}  // namespace

void MatchVirtualTasks(Profile* profile,
                       const std::vector<extensions::EntryInfo>& entries,
                       const std::vector<GURL>& file_urls,
                       const std::vector<std::string>& dlp_source_urls,
                       std::vector<FullTaskDescriptor>* result_list) {
  DCHECK_EQ(entries.size(), file_urls.size());
  if (entries.empty()) {
    return;
  }
  for (const VirtualTask* virtual_task : GetVirtualTasks()) {
    if (virtual_task->IsEnabled(profile) &&
        virtual_task->Matches(entries, file_urls)) {
      // TODO(b/284800493): Correct values below.
      result_list->emplace_back(
          TaskDescriptor{kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                         virtual_task->id()},
          virtual_task->title(), virtual_task->icon_url(),
          /* is_default=*/false,
          /* is_generic_file_handler=*/false,
          /* is_file_extension_match=*/false,
          virtual_task->IsDlpBlocked(dlp_source_urls));
    }
  }
}

bool ExecuteVirtualTask(Profile* profile,
                        const TaskDescriptor& task,
                        const std::vector<FileSystemURL>& file_urls) {
  auto* virtual_task = FindVirtualTask(task);
  if (!virtual_task || !virtual_task->IsEnabled(profile)) {
    return false;
  }
  return virtual_task->Execute(profile, task, file_urls);
}

bool IsVirtualTask(const TaskDescriptor& task) {
  return LooksLikeVirtualTask(task) &&
         base::Contains(GetVirtualTasks(), task.action_id, &VirtualTask::id);
}

VirtualTask* FindVirtualTask(const TaskDescriptor& task) {
  if (!LooksLikeVirtualTask(task)) {
    return nullptr;
  }
  const auto& tasks = GetVirtualTasks();
  auto itr = base::ranges::find(tasks, task.action_id, &VirtualTask::id);
  if (itr == tasks.end()) {
    return nullptr;
  }
  return *itr;
}

std::vector<VirtualTask*>& GetTestVirtualTasks() {
  static base::NoDestructor<std::vector<VirtualTask*>> virtual_tasks;
  return *virtual_tasks;
}

VirtualTask::VirtualTask() = default;
VirtualTask::~VirtualTask() = default;

bool VirtualTask::Matches(const std::vector<extensions::EntryInfo>& entries,
                          const std::vector<GURL>& file_urls) const {
  // Try to match mime types
  bool mime_types_matched =
      AllEntriesMatchAtLeastOneMimeType(entries, matcher_mime_types_);

  // Try to match extensions
  bool extensions_matched =
      AllUrlsMatchAtLeastOneFileExtension(file_urls, matcher_file_extensions_);

  // TODO(b/284800493): Should this be able to mix and match mimes and
  // extensions too?
  return mime_types_matched || extensions_matched;
}

}  // namespace file_manager::file_tasks
