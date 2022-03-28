// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

class HistoryClustersItemProperties {
    static final WritableObjectPropertyKey<Drawable> ICON_DRAWABLE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> URL = new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {ICON_DRAWABLE, TITLE, URL};

    @IntDef({HistoryClustersItemProperties.ItemType.VISIT})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        int VISIT = 1;
    }
}