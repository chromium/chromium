// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.followmanagement;

import android.graphics.Bitmap;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Items for the list view in the feed management activity.
 */
public class FollowManagementItemProperties {
    public static final int DEFAULT_ITEM_TYPE = 0;

    public static final WritableObjectPropertyKey<String> TITLE_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> URL_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> STATUS_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<OnClickListener> ON_CLICK_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Boolean> SUBSCRIBED_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<byte[]> ID_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Bitmap> FAVICON_KEY =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
            TITLE_KEY, URL_KEY, STATUS_KEY, ON_CLICK_KEY, SUBSCRIBED_KEY, ID_KEY, FAVICON_KEY};
}
