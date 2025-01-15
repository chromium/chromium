// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.graphics.drawable.Drawable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties for a button in the bookmark bar which provides users with bookmark access from top
 * chrome.
 */
class BookmarkBarButtonProperties {

    /** The callback to notify of bookmark bar button click events. */
    public static final WritableObjectPropertyKey<Runnable> CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();

    /** The icon to render in the bookmark bar button. */
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();

    /** The title to render in the bookmark bar button. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {CLICK_CALLBACK, ICON, TITLE};
}
