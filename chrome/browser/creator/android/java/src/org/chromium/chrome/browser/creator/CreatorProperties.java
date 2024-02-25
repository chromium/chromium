// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties used for the CreatorModel */
public class CreatorProperties {
    public static final WritableObjectPropertyKey<byte[]> WEB_FEED_ID_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> TITLE_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> URL_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> FORMATTED_URL_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Boolean> IS_FOLLOWED_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> ON_FOLLOW_CLICK_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> ON_FOLLOWING_CLICK_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Boolean> IS_TOOLBAR_VISIBLE_KEY =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        WEB_FEED_ID_KEY,
        TITLE_KEY,
        URL_KEY,
        FORMATTED_URL_KEY,
        IS_FOLLOWED_KEY,
        ON_FOLLOW_CLICK_KEY,
        ON_FOLLOWING_CLICK_KEY,
        IS_TOOLBAR_VISIBLE_KEY
    };
}
