// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_TEST_RECENT_FILE_MATCHER_H_
#define CHROME_BROWSER_ASH_FILEAPI_TEST_RECENT_FILE_MATCHER_H_

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Custom matcher that compares path and modified time with the specified
// values. The `path` must be base::FilePath. The `modified_time` must be
// base::Time.
MATCHER_P2(IsRecentFile, path, modified_time, "") {
  if (arg.url().path() != path) {
    *result_listener << arg.url().path() << " != " << path;
    return false;
  }
  if (arg.last_modified() != modified_time) {
    *result_listener << arg.last_modified() << " != " << modified_time;
    return false;
  }
  return true;
}

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_TEST_RECENT_FILE_MATCHER_H_
