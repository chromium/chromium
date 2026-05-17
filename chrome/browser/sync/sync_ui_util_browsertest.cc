// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include "build/build_config.h"
#include "components/sync/test/mock_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
using SyncUIUtilBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SyncUIUtilBrowserTest, ShowBookmarksLimitExceededHelp) {
  syncer::MockSyncService service;

  EXPECT_CALL(service,
              AcknowledgeBookmarksLimitExceededError(
                  syncer::SyncService::BookmarksLimitExceededHelpClickedSource::
                      kSettings));
  ShowBookmarksLimitExceededHelp(
      browser(), &service,
      syncer::SyncService::BookmarksLimitExceededHelpClickedSource::kSettings);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
