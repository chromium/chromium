// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.feedmanagement;

import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Items for the list view in the feed management activity.
 */
public class FeedManagementItemProperties {
    @IntDef({ListItemType.DEFAULT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ListItemType {
        int DEFAULT = 0;
    }
    public static final ReadableObjectPropertyKey<String> TITLE_KEY =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<String> DESCRIPTION_KEY =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<OnClickListener> ON_CLICK_KEY =
            new ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {TITLE_KEY, DESCRIPTION_KEY, ON_CLICK_KEY};
}
