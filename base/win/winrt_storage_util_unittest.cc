// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/winrt_storage_util.h"

#include <string.h>
#include <wrl/client.h>

#include <algorithm>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_hstring.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

TEST(WinrtStorageUtilTest, CreateBufferFromData) {
  ScopedCOMInitializer com_initializer(ScopedCOMInitializer::kMTA);

  const std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IBuffer> buffer;
  ASSERT_HRESULT_SUCCEEDED(CreateIBufferFromData(data, &buffer));

  base::span<uint8_t> span;
  ASSERT_HRESULT_SUCCEEDED(GetPointerToBufferData(buffer.Get(), span));

  ASSERT_EQ(span.size(), data.size());
  EXPECT_TRUE(std::ranges::equal(span, data));
}

}  // namespace base::win
