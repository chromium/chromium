// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/mock_search_picker_client.h"

#include <string>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::testing::Invoke;
using ::testing::SaveArg;

MockSearchPickerClient::MockSearchPickerClient() {
  ON_CALL(*this, StartCrosSearch)
      .WillByDefault(SaveArg<2>(&cros_search_callback_));
  ON_CALL(*this, FetchGifSearch)
      .WillByDefault(
          Invoke(this, &MockSearchPickerClient::FetchGifSearchToSetCallback));
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

MockSearchPickerClient::~MockSearchPickerClient() {}

MockSearchPickerClient::CrosSearchResultsCallback&
MockSearchPickerClient::cros_search_callback() {
  return cros_search_callback_;
}

MockSearchPickerClient::FetchGifsCallback&
MockSearchPickerClient::gif_search_callback() {
  return gif_search_callback_;
}

void MockSearchPickerClient::FetchGifSearchToSetCallback(
    const std::string& query,
    FetchGifsCallback callback) {
  gif_search_callback_ = std::move(callback);
}

}  // namespace ash
