// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_GUEST_OS_FILE_TASKS_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_GUEST_OS_FILE_TASKS_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"

class Profile;

namespace extensions {
struct EntryInfo;
}

namespace storage {
class FileSystemURL;
}

namespace file_manager {
namespace file_tasks {

// Guest OS apps all use the same action ID.
extern const char kGuestOsAppActionID[];

// Finds the Guest OS |app_ids| and |app_names| that can handle |entries|.
// VisibleForTesting.  Called by |FindGuestOsTasks|.
void FindGuestOsApps(Profile* profile,
                     const std::vector<extensions::EntryInfo>& entries,
                     const std::vector<GURL>& file_urls,
                     std::vector<std::string>* app_ids,
                     std::vector<std::string>* app_names,
                     std::vector<guest_os::VmType>* vm_types);

// Finds the Guest OS tasks that can handle |entries|, appends them to
// |result_list|, and calls back to |callback| once finished.
void FindGuestOsTasks(Profile* profile,
                      const std::vector<extensions::EntryInfo>& entries,
                      const std::vector<GURL>& file_urls,
                      std::vector<FullTaskDescriptor>* result_list,
                      base::OnceClosure completion_closure);

// Executes the specified task in a Guest OS VM.
void ExecuteGuestOsTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    FileTaskFinishedCallback done);

}  // namespace file_tasks
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_GUEST_OS_FILE_TASKS_H_
