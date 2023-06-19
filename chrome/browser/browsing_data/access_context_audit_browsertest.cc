// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

class AccessContextAuditBrowserTest : public PlatformBrowserTest {
 protected:
  base::FilePath db_path() {
    return chrome_test_utils::GetProfile(this)->GetPath().Append(
        FILE_PATH_LITERAL("AccessContextAudit"));
  }
};

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, PRE_DatabaseDeleted) {
  char data[] = "foo";
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::WriteFile(db_path(), data));
  ASSERT_TRUE(base::PathExists(db_path()));
}

IN_PROC_BROWSER_TEST_F(AccessContextAuditBrowserTest, DatabaseDeleted) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_FALSE(base::PathExists(db_path()));
}
