// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_TEST_HELPERS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_TEST_HELPERS_H_

class BookmarkMergedSurfaceService;

// Blocks until `service` finishes loading.
void WaitForBookmarkMergedSurfaceServiceToLoad(
    BookmarkMergedSurfaceService* service);

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_TEST_HELPERS_H_
