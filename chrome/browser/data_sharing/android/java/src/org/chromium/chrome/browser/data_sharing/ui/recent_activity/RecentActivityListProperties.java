// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import android.view.View.OnClickListener;
import android.widget.ImageView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for displaying a single recent activity row. */
@NullMarked
class RecentActivityListProperties {
    public static final WritableObjectPropertyKey<String> TITLE_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<DescriptionAndTimestamp>
            DESCRIPTION_AND_TIMESTAMP_TEXT = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Callback<ImageView>> FAVICON_PROVIDER =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Callback<ImageView>> AVATAR_PROVIDER =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<OnClickListener> ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                TITLE_TEXT,
                DESCRIPTION_AND_TIMESTAMP_TEXT,
                FAVICON_PROVIDER,
                AVATAR_PROVIDER,
                ON_CLICK_LISTENER
            };
}
