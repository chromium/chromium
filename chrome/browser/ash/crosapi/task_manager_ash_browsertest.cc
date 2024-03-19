// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/ash_requires_lacros_browsertestbase.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/task_manager/task_manager_tester.h"
#include "chrome/browser/ui/views/task_manager_view.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_observer.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/table/table_view.h"

using task_manager::browsertest_util::WaitForTaskManagerRows;

class TaskManagerAshBrowserTest
    : public crosapi::AshRequiresLacrosBrowserTestBase {
 public:
  task_manager::TaskManagerView* GetView() const {
    return task_manager::TaskManagerView::GetInstanceForTests();
  }
};

IN_PROC_BROWSER_TEST_F(TaskManagerAshBrowserTest,
                       OpenTaskManagerSelectActiveTab) {
  if (!HasLacrosArgument()) {
    GTEST_SKIP() << "TaskManagerAshBrowserTest not supported without Lacros.";
  }

  // Open task manager, verify the current Lacros tab is the selected task.
  auto tester =
      task_manager::TaskManagerTester::Create(base::RepeatingClosure());
  WaitForTaskManagerRows(1, u"Lacros: Tab: New Tab");
  std::optional<size_t> row = tester->GetRowForActiveTask();
  EXPECT_TRUE(row.has_value());
  EXPECT_EQ(u"Lacros: Tab: New Tab", tester->GetRowTitle(row.value()));

  task_manager::TaskManagerView* task_manager_view = GetView();
  CHECK(task_manager_view);

  views::TableView* tab_table = task_manager_view->tab_table_for_testing();
  CHECK(tab_table);

  // Verify the selected row is highlighted.
  EXPECT_EQ(row.value(), tab_table->GetFirstSelectedRow());

  // Create a new tab in the foreground, verify the new tab is not highlighted
  // since it's already highlighted before.
  crosapi::BrowserManager::Get()->OpenUrl(
      GURL("about:blank"), crosapi::mojom::OpenUrlFrom::kUnspecified,
      crosapi::mojom::OpenUrlParams::WindowOpenDisposition::kNewForegroundTab);
  WaitForTaskManagerRows(1, u"Lacros: Tab: about:blank");
  row = tester->GetRowForActiveTask();
  EXPECT_TRUE(row.has_value());
  EXPECT_EQ(u"Lacros: Tab: New Tab", tester->GetRowTitle(row.value()));
  EXPECT_EQ(row.value(), tab_table->GetFirstSelectedRow());
}
