// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

class HistoryClustersItemProperties {
    @IntDef({HistoryClustersItemProperties.ItemType.VISIT, ItemType.CLUSTER,
            ItemType.RELATED_SEARCHES, ItemType.TOGGLE, ItemType.PRIVACY_DISCLAIMER,
            ItemType.CLEAR_BROWSING_DATA, ItemType.MORE_PROGRESS})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        int VISIT = 1;
        int CLUSTER = 2;
        int RELATED_SEARCHES = 3;
        int TOGGLE = 4;
        int PRIVACY_DISCLAIMER = 5;
        int CLEAR_BROWSING_DATA = 6;
        int MORE_PROGRESS = 7;
    }

    static final WritableIntPropertyKey ACCESSIBILITY_STATE = new WritableIntPropertyKey();
    static final WritableObjectPropertyKey<Callback<String>> CHIP_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<OnClickListener> CLICK_HANDLER =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<ClusterVisit> CLUSTER_VISIT =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Boolean> DIVIDER_VISIBLE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<OnClickListener> END_BUTTON_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Drawable> END_BUTTON_DRAWABLE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Drawable> ICON_DRAWABLE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> LABEL = new WritableObjectPropertyKey<>();
    static final WritableIntPropertyKey PROGRESS_BUTTON_STATE = new WritableIntPropertyKey();
    static final WritableObjectPropertyKey<List<String>> RELATED_SEARCHES =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<CharSequence> TITLE = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<CharSequence> URL = new WritableObjectPropertyKey<>();
    static final WritableIntPropertyKey VISIBILITY = new WritableIntPropertyKey();

    static final PropertyKey[] ALL_KEYS = {ACCESSIBILITY_STATE, CHIP_CLICK_HANDLER, CLICK_HANDLER,
            CLUSTER_VISIT, DIVIDER_VISIBLE, END_BUTTON_CLICK_HANDLER, END_BUTTON_DRAWABLE,
            ICON_DRAWABLE, LABEL, PROGRESS_BUTTON_STATE, RELATED_SEARCHES, TITLE, URL, VISIBILITY};
}