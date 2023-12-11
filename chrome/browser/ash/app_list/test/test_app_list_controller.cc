// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/test/test_app_list_controller.h"

#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"

namespace test {

TestAppListController::TestAppListController() = default;
TestAppListController::~TestAppListController() = default;

ash::AppListClient* TestAppListController::GetClient() {
  return AppListClientImpl::GetInstance();
}

void TestAppListController::AddObserver(
    ash::AppListControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void TestAppListController::RemoveObserver(
    ash::AppListControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

aura::Window* TestAppListController::GetWindow() {
  NOTIMPLEMENTED();
  return nullptr;
}

void TestAppListController::ShowAppList(ash::AppListShowSource source) {
  last_open_source_ = source;
  visible_ = true;
  NotifyAppListVisibilityChanged();
}

ash::AppListShowSource TestAppListController::LastAppListShowSource() {
  DCHECK(last_open_source_.has_value());
  return last_open_source_.value();
}

void TestAppListController::DismissAppList() {
  visible_ = false;
  NotifyAppListVisibilityChanged();
}

bool TestAppListController::IsVisible(
    const std::optional<int64_t>& display_id) {
  return visible_;
}

bool TestAppListController::IsVisible() {
  return visible_;
}

void TestAppListController::UpdateAppListWithNewTemporarySortOrder(
    const std::optional<ash::AppListSortOrder>& new_order,
    bool animate,
    base::OnceClosure update_position_closure) {
  if (!update_position_closure) {
    DCHECK(!animate);
    return;
  }

  DCHECK(animate);
  std::move(update_position_closure).Run();
}

void TestAppListController::NotifyAppListVisibilityChanged() {
  for (auto& observer : observers_) {
    observer.OnAppListVisibilityWillChange(visible_, /*display_id=*/-1);
  }
  for (auto& observer : observers_) {
    observer.OnAppListVisibilityWillChange(visible_,
                                           /*display_id=*/-1);
  }
}

}  // namespace test
