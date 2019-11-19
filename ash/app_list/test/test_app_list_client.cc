// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test/test_app_list_client.h"

#include <utility>

namespace ash {

TestAppListClient::TestAppListClient() = default;
TestAppListClient::~TestAppListClient() = default;

void TestAppListClient::GetSearchResultContextMenuModel(
    const std::string& result_id,
    GetContextMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

void TestAppListClient::GetContextMenuModel(
    int profile_id,
    const std::string& id,
    GetContextMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

}  // namespace ash
