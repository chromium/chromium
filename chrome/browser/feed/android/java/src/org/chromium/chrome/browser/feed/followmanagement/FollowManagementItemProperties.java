// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import android.graphics.Bitmap;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Items for the list view in the feed management activity. */
public class FollowManagementItemProperties {
    public static final int DEFAULT_ITEM_TYPE = 0;
    public static final int EMPTY_ITEM_TYPE = 1;
    public static final int LOADING_ITEM_TYPE = 2;

    public static final WritableObjectPropertyKey<String> TITLE_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> URL_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> STATUS_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> ON_CLICK_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Boolean> SUBSCRIBED_KEY =
            new WritableObjectPropertyKey<>();
    // Whether the subscribe state is transitioning. The user cannot attempt to subscribe or
    // unsubscribe while this is true.
    public static final WritableObjectPropertyKey<Boolean> CHECKBOX_ENABLED_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<byte[]> ID_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Bitmap> FAVICON_KEY =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        TITLE_KEY,
        URL_KEY,
        STATUS_KEY,
        ON_CLICK_KEY,
        SUBSCRIBED_KEY,
        CHECKBOX_ENABLED_KEY,
        ID_KEY,
        FAVICON_KEY
    };
}
