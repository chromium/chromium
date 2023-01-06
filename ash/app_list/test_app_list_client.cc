// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test_app_list_client.h"

#include <utility>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TestAppListClient::OnZeroStateSearchDone,
                       weak_factory_.GetWeakPtr(), std::move(on_done)),
        base::Milliseconds(1));
  }
}

void TestAppListClient::StartSearch(const std::u16string& trimmed_query) {
  search_queries_.push_back(trimmed_query);
  if (search_callback_)
    search_callback_.Run(trimmed_query);
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
    AppListItemContext item_context,
    GetContextMenuModelCallback callback) {
  auto model = std::make_unique<ui::SimpleMenuModel>(/*delegate=*/nullptr);
  model->AddItem(/*command_id=*/0, u"Menu item");
  std::move(callback).Run(std::move(model));
}

AppListNotifier* TestAppListClient::GetNotifier() {
  return nullptr;
}

std::vector<TestAppListClient::SearchResultActionId>
TestAppListClient::GetAndResetInvokedResultActions() {
  std::vector<SearchResultActionId> result;
  result.swap(invoked_result_actions_);
  return result;
}

std::vector<std::u16string> TestAppListClient::GetAndResetPastSearchQueries() {
  std::vector<std::u16string> result;
  result.swap(search_queries_);
  return result;
}

ash::AppListSortOrder TestAppListClient::GetPermanentSortingOrder() const {
  NOTIMPLEMENTED();
  return ash::AppListSortOrder::kCustom;
}

void TestAppListClient::CommitTemporarySortOrder() {
  // Committing the temporary sort order should not introduce item reorder so
  // reset the sort order without reorder animation.
  AppListController::Get()->UpdateAppListWithNewTemporarySortOrder(
      /*new_order=*/absl::nullopt, /*animate=*/false, base::NullCallback());
}

void TestAppListClient::OnZeroStateSearchDone(base::OnceClosure on_done) {
  zero_state_search_done_count_++;
  std::move(on_done).Run();
}

}  // namespace ash
