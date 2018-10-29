// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_tester.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/table_model_observer.h"

namespace task_manager {

// Temporarily intercepts the calls between a TableModel and its Observer,
// running |callback| whenever anything happens.
class ScopedInterceptTableModelObserver : public ui::TableModelObserver {
 public:
  ScopedInterceptTableModelObserver(
      ui::TableModel* model_to_intercept,
      ui::TableModelObserver* real_table_model_observer,
      const base::Closure& callback)
      : model_to_intercept_(model_to_intercept),
        real_table_model_observer_(real_table_model_observer),
        callback_(callback) {
    model_to_intercept_->SetObserver(this);
  }

  ~ScopedInterceptTableModelObserver() override {
    model_to_intercept_->SetObserver(real_table_model_observer_);
  }

  // ui::TableModelObserver:
  void OnModelChanged() override {
    real_table_model_observer_->OnModelChanged();
    callback_.Run();
  }
  void OnItemsChanged(int start, int length) override {
    real_table_model_observer_->OnItemsChanged(start, length);
    callback_.Run();
  }
  void OnItemsAdded(int start, int length) override {
    real_table_model_observer_->OnItemsAdded(start, length);
    callback_.Run();
  }
  void OnItemsRemoved(int start, int length) override {
    real_table_model_observer_->OnItemsRemoved(start, length);
    callback_.Run();
  }

 private:
  ui::TableModel* model_to_intercept_;
  ui::TableModelObserver* real_table_model_observer_;
  base::Closure callback_;
};

namespace {

// Returns the TaskManagerTableModel for the the visible NewTaskManagerView.
TaskManagerTableModel* GetRealModel() {
  return chrome::ShowTaskManager(nullptr);
}

}  // namespace

TaskManagerTester::TaskManagerTester(const base::Closure& on_resource_change)
    : model_(GetRealModel()) {
  // Eavesdrop the model->view conversation, since the model only supports
  // single observation.
  if (!on_resource_change.is_null()) {
    interceptor_.reset(new ScopedInterceptTableModelObserver(
        model_, model_->table_model_observer_, on_resource_change));
  }
}

TaskManagerTester::~TaskManagerTester() {
  CHECK_EQ(GetRealModel(), model_) << "Task Manager should not be hidden "
                                      "while TaskManagerTester is alive. "
                                      "This indicates a test bug.";
}

// TaskManagerTester:
int TaskManagerTester::GetRowCount() {
  return model_->RowCount();
}

base::string16 TaskManagerTester::GetRowTitle(int row) {
  return model_->GetText(row, IDS_TASK_MANAGER_TASK_COLUMN);
}

void TaskManagerTester::ToggleColumnVisibility(ColumnSpecifier column) {
  int column_id = 0;
  switch (column) {
    case ColumnSpecifier::COLUMN_NONE:
      return;
    case ColumnSpecifier::PROCESS_ID:
      column_id = IDS_TASK_MANAGER_PROCESS_ID_COLUMN;
      break;
    case ColumnSpecifier::MEMORY_FOOTPRINT:
      column_id = IDS_TASK_MANAGER_MEM_FOOTPRINT_COLUMN;
      break;
    case ColumnSpecifier::SQLITE_MEMORY_USED:
      column_id = IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN;
      break;
    case ColumnSpecifier::V8_MEMORY_USED:
    case ColumnSpecifier::V8_MEMORY:
      column_id = IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN;
      break;
    case ColumnSpecifier::IDLE_WAKEUPS:
      column_id = IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN;
      break;
    case ColumnSpecifier::TOTAL_NETWORK_USE:
    case ColumnSpecifier::NETWORK_USE:
      column_id = IDS_TASK_MANAGER_NET_COLUMN;
      break;
  }
  model_->ToggleColumnVisibility(column_id);
}

int64_t TaskManagerTester::GetColumnValue(ColumnSpecifier column, int row) {
  TaskId task_id = model_->tasks_[row];
  int64_t value = 0;
  int64_t ignored = 0;
  bool success = false;

  switch (column) {
    case ColumnSpecifier::COLUMN_NONE:
      break;
    case ColumnSpecifier::MEMORY_FOOTPRINT:
      value = task_manager()->GetMemoryFootprintUsage(task_id);
      success = true;
      break;
    case ColumnSpecifier::PROCESS_ID:
      value = static_cast<int64_t>(task_manager()->GetProcessId(task_id));
      success = true;
      break;
    case ColumnSpecifier::V8_MEMORY:
      success = task_manager()->GetV8Memory(task_id, &value, &ignored);
      break;
    case ColumnSpecifier::V8_MEMORY_USED:
      success = task_manager()->GetV8Memory(task_id, &ignored, &value);
      break;
    case ColumnSpecifier::SQLITE_MEMORY_USED:
      value = task_manager()->GetSqliteMemoryUsed(task_id);
      success = true;
      break;
    case ColumnSpecifier::IDLE_WAKEUPS:
      value = task_manager()->GetIdleWakeupsPerSecond(task_id);
      success = true;
      break;
    case ColumnSpecifier::NETWORK_USE:
      value = task_manager()->GetNetworkUsage(task_id);
      success = true;
      break;
    case ColumnSpecifier::TOTAL_NETWORK_USE:
      value = task_manager()->GetCumulativeNetworkUsage(task_id);
      success = true;
      break;
  }
  if (!success)
    return 0;
  return value;
}

SessionID TaskManagerTester::GetTabId(int row) {
  TaskId task_id = model_->tasks_[row];
  return task_manager()->GetTabId(task_id);
}

void TaskManagerTester::Kill(int row) {
  model_->KillTask(row);
}

void TaskManagerTester::GetRowsGroupRange(int row,
                                          int* out_start,
                                          int* out_length) {
  return model_->GetRowsGroupRange(row, out_start, out_length);
}

TaskManagerInterface* TaskManagerTester::task_manager() {
  return model_->observed_task_manager();
}

// static
std::unique_ptr<TaskManagerTester> TaskManagerTester::Create(
    const base::Closure& callback) {
  return base::WrapUnique(new TaskManagerTester(callback));
}

}  // namespace task_manager
