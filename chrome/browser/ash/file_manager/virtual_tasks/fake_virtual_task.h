// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_VIRTUAL_TASKS_FAKE_VIRTUAL_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_VIRTUAL_TASKS_FAKE_VIRTUAL_TASK_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/file_manager/virtual_file_tasks.h"

namespace file_manager::file_tasks {

class FakeVirtualTask : public VirtualTask {
 public:
  // If `execute_cb` is not provided, Execute() will return true by default.
  explicit FakeVirtualTask(
      const std::string& id,
      bool enabled = true,
      bool matches = true,
      bool is_dlp_blocked = false,
      base::RepeatingCallback<bool()> execute_cb = base::NullCallback());
  ~FakeVirtualTask() override;

  bool Execute(Profile* profile,
               const TaskDescriptor& task,
               const std::vector<FileSystemURL>& file_urls) const override;
  bool IsEnabled(Profile* profile) const override;
  bool Matches(const std::vector<extensions::EntryInfo>& entries,
               const std::vector<GURL>& file_urls) const override;

  std::string id() const override;
  GURL icon_url() const override;
  std::string title() const override;
  bool IsDlpBlocked(
      const std::vector<std::string>& dlp_source_urls) const override;

 private:
  const std::string id_;

  const bool enabled_;
  const bool matches_;
  const bool is_dlp_blocked_;
  const base::RepeatingCallback<bool()> execute_cb_;
};

}  // namespace file_manager::file_tasks

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_VIRTUAL_TASKS_FAKE_VIRTUAL_TASK_H_
