// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_UNITTEST_UTILS_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_UNITTEST_UTILS_H_

#include <stddef.h>

struct ImportedBookmarkEntry;

const int kMaxPathSize = 5;

struct BookmarkInfo {
  const bool in_toolbar;
  const size_t path_size;
  const char* path[kMaxPathSize];
  const wchar_t* title;
  const char* url;
};

// Generates an assertion error if |entry| is not equal to |expected|. Wrap this
// method in (ASSERT|EXPECT)_NO_FATAL_FAILURE to catch the error if one is
// generated.
void TestEqualBookmarkEntry(const ImportedBookmarkEntry& entry,
                            const BookmarkInfo& expected);

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_UNITTEST_UTILS_H_
