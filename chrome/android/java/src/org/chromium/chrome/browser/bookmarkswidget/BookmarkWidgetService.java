// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarkswidget;

import org.chromium.chrome.browser.base.SplitCompatRemoteViewsService;
import org.chromium.chrome.browser.base.SplitCompatUtils;

/** See {@link BookmarkWidgetServiceImpl}. */
public class BookmarkWidgetService extends SplitCompatRemoteViewsService {
    public BookmarkWidgetService() {
        super(SplitCompatUtils.getIdentifierName(
                "org.chromium.chrome.browser.bookmarkswidget.BookmarkWidgetServiceImpl"));
    }
}
