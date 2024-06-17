// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and use spans.
#pragma allow_unsafe_buffers
#endif

#include "base/profiler/chrome_unwind_info_android.h"

#include <tuple>

#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

bool operator==(const FunctionTableEntry& e1, const FunctionTableEntry& e2) {
  return std::tie(e1.function_start_address_page_instruction_offset,
                  e1.function_offset_table_byte_index) ==
         std::tie(e2.function_start_address_page_instruction_offset,
                  e2.function_offset_table_byte_index);
}

template <class T,
          size_t E1,
          size_t E2,
          typename InternalPtrType1,
          typename InternalPtrType2>
void ExpectSpanSizeAndContentsEqual(span<T, E1, InternalPtrType1> actual,
                                    span<T, E2, InternalPtrType2> expected) {
  EXPECT_EQ(actual.size(), expected.size());
  if (actual.size() != expected.size()) {
    return;
  }

  for (size_t i = 0; i < actual.size(); i++) {
    EXPECT_EQ(actual[i], expected[i]);
  }
}

TEST(ChromeUnwindInfoAndroidTest, CreateUnwindInfo) {
  ChromeUnwindInfoHeaderAndroid header = {
      /* page_table_byte_offset */ 64,
      /* page_table_entries */ 1,

      /* function_table_byte_offset */ 128,
      /* function_table_entries */ 2,

      /* function_offset_table_byte_offset */ 192,
      /* function_offset_table_size_in_bytes */ 3,

      /* unwind_instruction_table_byte_offset */ 256,
      /* unwind_instruction_table_size_in_bytes */ 4,
  };

  uint8_t data[512] = {};
  // Note: `CreateChromeUnwindInfoAndroid` is not expected to verify the content
  // of each unwind table.
  const uint32_t page_table[] = {1};
  const FunctionTableEntry function_table[] = {{0, 2}, {0, 3}};
  const uint8_t function_offset_table[] = {3, 3, 3};
  const uint8_t unwind_instruction_table[] = {4, 4, 4, 4};

  ASSERT_LT(sizeof(ChromeUnwindInfoHeaderAndroid), 64ul);
  memcpy(&data[0], &header, sizeof(ChromeUnwindInfoHeaderAndroid));

  memcpy(&data[header.page_table_byte_offset], page_table, sizeof(page_table));
  memcpy(&data[header.function_table_byte_offset], function_table,
         sizeof(function_table));
  memcpy(&data[header.function_offset_table_byte_offset], function_offset_table,
         sizeof(function_offset_table));
  memcpy(&data[header.unwind_instruction_table_byte_offset],
         unwind_instruction_table, sizeof(unwind_instruction_table));

  ChromeUnwindInfoAndroid unwind_info = CreateChromeUnwindInfoAndroid(data);

  ASSERT_EQ(&data[64],
            reinterpret_cast<const uint8_t*>(&unwind_info.page_table[0]));
  ASSERT_EQ(&data[128],
            reinterpret_cast<const uint8_t*>(&unwind_info.function_table[0]));
  ASSERT_EQ(&data[192], reinterpret_cast<const uint8_t*>(
                            &unwind_info.function_offset_table[0]));
  ASSERT_EQ(&data[256], reinterpret_cast<const uint8_t*>(
                            &unwind_info.unwind_instruction_table[0]));

  ExpectSpanSizeAndContentsEqual(unwind_info.page_table, make_span(page_table));
  ExpectSpanSizeAndContentsEqual(unwind_info.function_table,
                                 make_span(function_table));
  ExpectSpanSizeAndContentsEqual(unwind_info.function_offset_table,
                                 make_span(function_offset_table));
  ExpectSpanSizeAndContentsEqual(unwind_info.unwind_instruction_table,
                                 make_span(unwind_instruction_table));
}

}  // namespace base
