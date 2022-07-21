// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_OFFICE_TASK_SELECTION_HELPER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_OFFICE_TASK_SELECTION_HELPER_H_

#include <set>
#include <string>
#include <vector>

#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "components/drive/file_errors.h"
#include "extensions/browser/entry_info.h"

namespace file_manager::file_tasks {

struct FullTaskDescriptor;

// Helper class that determines what Files app task should be used to handle
// Office files, if any.
class OfficeTaskSelectionHelper {
 public:
  OfficeTaskSelectionHelper(
      Profile* profile,
      const std::vector<extensions::EntryInfo>& entries,
      std::unique_ptr<std::vector<FullTaskDescriptor>> result_list,
      std::set<std::string> disabled_actions);

  OfficeTaskSelectionHelper(const OfficeTaskSelectionHelper& other) = delete;
  OfficeTaskSelectionHelper& operator=(const OfficeTaskSelectionHelper& other) =
      delete;

  ~OfficeTaskSelectionHelper();

  // Starts processing the selected entries to determine what Office file
  // handler should be used.
  void Run(base::OnceClosure callback);

  Profile* profile;
  const std::vector<extensions::EntryInfo> entries;
  std::unique_ptr<std::vector<FullTaskDescriptor>> result_list;
  std::set<std::string> disabled_actions;

 private:
  bool IsCandidateWebDriveOffice();
  bool IsCandidateUploadOfficeToDrive();
  void InvalidateCandidate();
  std::string ExtensionToWebDriveOfficeActionId(std::string extension);
  bool SetCandidateActionId();
  void AdjustTasks();
  void ProcessNextEntryForAlternateUrl(size_t entry_index);
  void OnGetDriveFsMetadataForWebDriveOffice(
      size_t entry_index,
      drive::FileError error,
      drivefs::mojom::FileMetadataPtr metadata);
  void EndAdjustTasks();

  std::string candidate_office_action_id_;
  base::OnceClosure callback_;
  base::WeakPtrFactory<OfficeTaskSelectionHelper> weak_factory_{this};
};

}  // namespace file_manager::file_tasks

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_OFFICE_TASK_SELECTION_HELPER_H_
