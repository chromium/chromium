// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test_app_list_client.h"

#include <utility>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

TestAppListClient::TestAppListClient() = default;

TestAppListClient::~TestAppListClient() = default;

void TestAppListClient::StartZeroStateSearch(base::OnceClosure on_done,
                                             base::TimeDelta timeout) {
  start_zero_state_search_count_++;
  if (run_zero_state_callback_immediately_) {
    // Most unit tests generally expect the launcher to open immediately, so run
    // the callback synchronously.
    std::move(on_done).Run();
  } else {
    // Simulate production behavior, which collects the results asynchronously.
    // Bounce through OnZeroStateSearchDone() to count calls, so that tests can
    // assert that the callback happened.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TestAppListClient::OnZeroStateSearchDone,
                       weak_factory_.GetWeakPtr(), std::move(on_done)),
        base::Milliseconds(1));
  }
}

void TestAppListClient::StartSearch(const std::u16string& trimmed_query) {
  last_search_query_ = trimmed_query;
}

void TestAppListClient::OpenSearchResult(int profile_id,
                                         const std::string& result_id,
                                         int event_flags,
                                         AppListLaunchedFrom launched_from,
                                         AppListLaunchType launch_type,
                                         int suggestion_index,
                                         bool launch_as_default) {
  last_opened_search_result_ = result_id;
}

void TestAppListClient::InvokeSearchResultAction(
    const std::string& result_id,
    SearchResultActionType action) {
  invoked_result_actions_.emplace_back(result_id, action);
}

void TestAppListClient::GetSearchResultContextMenuModel(
    const std::string& result_id,
    GetContextMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

void TestAppListClient::ActivateItem(int profile_id,
                                     const std::string& id,
                                     int event_flags,
                                     ash::AppListLaunchedFrom launched_from) {
  activate_item_count_++;
  activate_item_last_id_ = id;
}

void TestAppListClient::GetContextMenuModel(
    int profile_id,
    const std::string& id,
    bool add_sort_options,
    GetContextMenuModelCallback callback) {
  auto model = std::make_unique<ui::SimpleMenuModel>(/*delegate=*/nullptr);
  model->AddItem(/*command_id=*/0, u"Menu item");
  std::move(callback).Run(std::move(model));
}

AppListNotifier* TestAppListClient::GetNotifier() {
  return nullptr;
}

std::vector<TestAppListClient::SearchResultActionId>
TestAppListClient::GetAndClearInvokedResultActions() {
  std::vector<SearchResultActionId> result;
  result.swap(invoked_result_actions_);
  return result;
}

ash::AppListSortOrder TestAppListClient::GetPermanentSortingOrder() const {
  NOTIMPLEMENTED();
  return ash::AppListSortOrder::kCustom;
}

void TestAppListClient::OnZeroStateSearchDone(base::OnceClosure on_done) {
  zero_state_search_done_count_++;
  std::move(on_done).Run();
}

}  // namespace ash
