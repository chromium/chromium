// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_TESTER_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_TESTER_H_

#include <stdint.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "components/sessions/core/session_id.h"

namespace task_manager {

class ScopedInterceptTableModelObserver;
class TaskManagerInterface;
class TaskManagerTableModel;

// An adapter to simplify testing the task manager.
class TaskManagerTester {
 public:
  using ColumnSpecifier = browsertest_util::ColumnSpecifier;

  ~TaskManagerTester();

  // Creates a TaskManagerTester backed by the current task manager. The task
  // manager should already be visible when you call this function.
  // |on_resource_change|, if not a null callback, will be invoked when the
  // underlying model changes.
  static std::unique_ptr<TaskManagerTester> Create(
      const base::Closure& on_resource_change);

  // Get the number of rows currently in the task manager.
  int GetRowCount();

  // Get the title text of a particular |row|.
  base::string16 GetRowTitle(int row);

  // Hide or show a column. If a column is not visible its stats are not
  // necessarily gathered.
  void ToggleColumnVisibility(ColumnSpecifier column);

  // Get the value of a column as an int64. Memory values are in bytes.
  int64_t GetColumnValue(ColumnSpecifier column, int row);

  // If |row| is associated with a WebContents, return its SessionID. Otherwise,
  // return SessionID::InvalidValue().
  SessionID GetTabId(int row);

  // Kill the process of |row|.
  void Kill(int row);

  // Gets the start index and length of the group to which the task at
  // |row_index| belongs.
  void GetRowsGroupRange(int row, int* out_start, int* out_length);

 private:
  explicit TaskManagerTester(const base::Closure& on_resource_change);

  TaskManagerInterface* task_manager();

  TaskManagerTableModel* model_;
  std::unique_ptr<ScopedInterceptTableModelObserver> interceptor_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_TESTER_H_
