// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/virtual_file_tasks.h"

#include <initializer_list>
#include <vector>

#include "ash/webui/file_manager/url_constants.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/network_service_instance.h"

namespace file_manager::file_tasks {

std::vector<VirtualTask*>& GetTestVirtualTasks() {
  static base::NoDestructor<std::vector<VirtualTask*>> virtual_tasks;
  return *virtual_tasks;
}

// The set of virtual tasks is statically determined. Tasks can turn themselves
// on or off dynamically by implementing |IsEnabled()|.
const std::vector<VirtualTask*> GetVirtualTasks() {
  static const base::NoDestructor<std::vector<VirtualTask*>> virtual_tasks(
      std::initializer_list<VirtualTask*>({}));
  if (!GetTestVirtualTasks().empty()) {
    return GetTestVirtualTasks();
  }
  return *virtual_tasks;
}

void FindVirtualTasks(Profile* profile,
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
        virtual_task->Matches(entries, file_urls, dlp_source_urls)) {
      // TODO(b/284800493): Correct values below.
      result_list->emplace_back(
          TaskDescriptor{kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                         virtual_task->id()},
          virtual_task->title(), virtual_task->icon_url(),
          /* is_default=*/false,
          /* is_generic_file_handler=*/false,
          /* is_file_extension_match=*/false,
          /* is_dlp_blocked=*/false);
    }
  }
}

bool ExecuteVirtualTask(Profile* profile,
                        const TaskDescriptor& task,
                        const std::vector<FileSystemURL>& file_urls,
                        gfx::NativeWindow modal_parent) {
  if (!IsVirtualTask(task)) {
    return false;
  }

  for (const VirtualTask* virtual_task : GetVirtualTasks()) {
    if (virtual_task->id() == task.action_id &&
        virtual_task->IsEnabled(profile)) {
      return virtual_task->Execute(profile, task, file_urls, modal_parent);
    }
  }
  return false;
}

bool IsVirtualTask(const TaskDescriptor& task) {
  if (task.app_id != kFileManagerSwaAppId ||
      task.task_type != TASK_TYPE_WEB_APP) {
    return false;
  }

  for (const VirtualTask* virtual_task : GetVirtualTasks()) {
    if (virtual_task->id() == task.action_id) {
      return true;
    }
  }
  return false;
}

VirtualTask::VirtualTask() = default;
VirtualTask::~VirtualTask() = default;

}  // namespace file_manager::file_tasks
