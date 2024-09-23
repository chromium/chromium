// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/importer/importer_unittest_utils.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

void TestEqualBookmarkEntry(const ImportedBookmarkEntry& entry,
                            const BookmarkInfo& expected) {
  ASSERT_EQ(base::WideToUTF16(expected.title), entry.title);
  ASSERT_EQ(expected.in_toolbar, entry.in_toolbar) << entry.title;
  ASSERT_EQ(expected.path_size, entry.path.size()) << entry.title;
  ASSERT_EQ(expected.url, entry.url.spec()) << entry.title;
  for (size_t i = 0; i < expected.path_size; ++i) {
    ASSERT_EQ(base::ASCIIToUTF16(expected.path[i]),
              entry.path[i]) << entry.title;
  }
}
