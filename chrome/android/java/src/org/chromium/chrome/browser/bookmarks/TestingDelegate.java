// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.Nullable;

import org.chromium.components.bookmarks.BookmarkId;

/** Exposes business logic methods to the various bookmark integration. */
public interface TestingDelegate {
    BookmarkId getIdByPositionForTesting(int position);

    void searchForTesting(@Nullable String query);

    // TODO(crbug.com/40264714): Delete this method.
    void simulateSignInForTesting();
}
