// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/virtual_tasks/fake_virtual_task.h"

namespace file_manager::file_tasks {

FakeVirtualTask::FakeVirtualTask(const std::string& id,
                                 bool enabled,
                                 bool matches,
                                 bool is_dlp_blocked,
                                 base::RepeatingCallback<bool()> execute_cb)
    : id_(id),
      enabled_(enabled),
      matches_(matches),
      is_dlp_blocked_(is_dlp_blocked),
      execute_cb_(std::move(execute_cb)) {}

FakeVirtualTask::~FakeVirtualTask() = default;

bool FakeVirtualTask::Execute(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<FileSystemURL>& file_urls) const {
  return execute_cb_ ? execute_cb_.Run() : true;
}

bool FakeVirtualTask::IsEnabled(Profile* profile) const {
  return enabled_;
}

bool FakeVirtualTask::Matches(const std::vector<extensions::EntryInfo>& entries,
                              const std::vector<GURL>& file_urls) const {
  return matches_;
}

std::string FakeVirtualTask::id() const {
  return id_;
}

GURL FakeVirtualTask::icon_url() const {
  return GURL();
}

std::string FakeVirtualTask::title() const {
  return id() + "title";
}

bool FakeVirtualTask::IsDlpBlocked(
    const std::vector<std::string>& dlp_source_urls) const {
  return is_dlp_blocked_;
}

}  // namespace file_manager::file_tasks
