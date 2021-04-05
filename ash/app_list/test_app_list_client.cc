// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test_app_list_client.h"

#include <utility>

#include "ui/base/models/simple_menu_model.h"

namespace ash {

TestAppListClient::TestAppListClient() = default;
TestAppListClient::~TestAppListClient() = default;

void TestAppListClient::InvokeSearchResultAction(const std::string& result_id,
                                                 int action_index) {
  invoked_result_actions_.push_back(std::make_pair(result_id, action_index));
}

void TestAppListClient::GetSearchResultContextMenuModel(
    const std::string& result_id,
    GetContextMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

void TestAppListClient::GetContextMenuModel(
    int profile_id,
    const std::string& id,
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

}  // namespace ash
