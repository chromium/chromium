// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_TEST_HELPERS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_TEST_HELPERS_H_

#include "chrome/browser/bookmarks/permanent_folder_ordering_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

class BookmarkMergedSurfaceService;

// Blocks until `service` finishes loading.
void WaitForBookmarkMergedSurfaceServiceToLoad(
    BookmarkMergedSurfaceService* service);

class MockPermanentFolderOrderingTrackerDelegate
    : public PermanentFolderOrderingTracker::Delegate {
 public:
  MockPermanentFolderOrderingTrackerDelegate();
  ~MockPermanentFolderOrderingTrackerDelegate() override;

  MOCK_METHOD(void, TrackedOrderingChanged, (), (override));
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_TEST_HELPERS_H_
