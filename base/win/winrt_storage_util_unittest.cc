// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/winrt_storage_util.h"

#include <string.h>
#include <wrl/client.h>

#include <vector>

#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_hstring.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

TEST(WinrtStorageUtilTest, CreateBufferFromData) {
  ScopedCOMInitializer com_initializer(ScopedCOMInitializer::kMTA);

  const std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IBuffer> buffer;
  ASSERT_HRESULT_SUCCEEDED(
      CreateIBufferFromData(data.data(), data.size(), &buffer));

  uint8_t* p_buffer_data;
  uint32_t length;
  ASSERT_HRESULT_SUCCEEDED(
      GetPointerToBufferData(buffer.Get(), &p_buffer_data, &length));

  ASSERT_EQ(data.size(), length);
  EXPECT_EQ(0, memcmp(p_buffer_data, data.data(), data.size()));
}

}  // namespace win
}  // namespace base
