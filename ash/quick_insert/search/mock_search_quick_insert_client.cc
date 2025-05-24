// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/search/mock_search_quick_insert_client.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::testing::Invoke;
using ::testing::SaveArg;

MockSearchQuickInsertClient::MockSearchQuickInsertClient() {
  ON_CALL(*this, StartCrosSearch)
      .WillByDefault(SaveArg<2>(&cros_search_callback_));
  ON_CALL(*this, GetSharedURLLoaderFactory).WillByDefault([]() {
    ADD_FAILURE()
        << "GetSharedURLLoaderFactory should not be called in this unittest";
    return nullptr;
  });
  ON_CALL(*this, CacheEditorContext).WillByDefault([]() {
    ADD_FAILURE() << "CacheEditorContext should not be called in this unittest";
    return ShowEditorCallback();
  });
}

MockSearchQuickInsertClient::~MockSearchQuickInsertClient() {}

MockSearchQuickInsertClient::CrosSearchResultsCallback&
MockSearchQuickInsertClient::cros_search_callback() {
  return cros_search_callback_;
}

}  // namespace ash
