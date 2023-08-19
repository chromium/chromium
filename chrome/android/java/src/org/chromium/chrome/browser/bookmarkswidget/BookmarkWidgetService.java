// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarkswidget;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatRemoteViewsService;

/** See {@link BookmarkWidgetServiceImpl}. */
public class BookmarkWidgetService extends SplitCompatRemoteViewsService {
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.bookmarkswidget.BookmarkWidgetServiceImpl";

    public BookmarkWidgetService() {
        super(sImplClassName);
    }
}
