// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_TEST_APP_LIST_TEST_HELPER_H_
#define ASH_APP_LIST_TEST_APP_LIST_TEST_HELPER_H_

#include <memory>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/test/test_app_list_client.h"

namespace ash {

class AppListControllerImpl;
class AppListView;
enum class AppListViewState;

class AppListTestHelper {
 public:
  AppListTestHelper();
  ~AppListTestHelper();

  // Show the app list in |display_id|, and wait until animation finishes.
  // Note: we usually don't care about the show source in tests.
  void ShowAndRunLoop(uint64_t display_id);

  // Show the app list in |display_id|.
  void Show(uint64_t display_id);

  // Show the app list in |display_id| triggered with |show_source|, and wait
  // until animation finishes.
  void ShowAndRunLoop(uint64_t display_id, AppListShowSource show_source);

  // Dismiss the app list, and wait until animation finishes.
  void DismissAndRunLoop();

  // Dismiss the app list.
  void Dismiss();

  // Toggle the app list in |display_id|, and wait until animation finishes.
  // Note: we usually don't care about the show source in tests.
  void ToggleAndRunLoop(uint64_t display_id);

  // Toggle the app list in |display_id| triggered with |show_source|, and wait
  // until animation finishes.
  void ToggleAndRunLoop(uint64_t display_id, AppListShowSource show_source);

  // Check the visibility value of the app list and its target.
  // Fails in tests if either one doesn't match |visible|,.
  void CheckVisibility(bool visible);

  // Check the current app list view state.
  void CheckState(ash::AppListViewState state);

  // Run all pending in message loop to wait for animation to finish.
  void WaitUntilIdle();

  AppListView* GetAppListView();

 private:
  AppListControllerImpl* app_list_controller_ = nullptr;
  std::unique_ptr<TestAppListClient> app_list_client_;

  DISALLOW_COPY_AND_ASSIGN(AppListTestHelper);
};

}  // namespace ash

#endif  // ASH_APP_LIST_TEST_APP_LIST_TEST_HELPER_H_
