// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Contains model properties for a single search list item in Continuous Search Navigation.
 */
class ContinuousSearchListProperties {
    @IntDef({ListItemType.DEPRECATED_PROVIDER, ListItemType.SEARCH_RESULT, ListItemType.AD})
    @Retention(RetentionPolicy.SOURCE)

    public @interface ListItemType {
        int DEPRECATED_PROVIDER = 0;
        int SEARCH_RESULT = 1;
        int AD = 2;
    }

    /**
     * Properties used for individual items shown in the RecyclerView.
     */
    static class ListItemProperties {
        public static final WritableObjectPropertyKey<String> LABEL =
                new WritableObjectPropertyKey<>();
        public static final WritableObjectPropertyKey<GURL> URL = new WritableObjectPropertyKey<>();
        public static final WritableBooleanPropertyKey IS_SELECTED =
                new WritableBooleanPropertyKey();
        public static final WritableIntPropertyKey BORDER_COLOR = new WritableIntPropertyKey();
        public static final WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
                new WritableObjectPropertyKey<>();
        public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();
        public static final WritableIntPropertyKey PRIMARY_TEXT_STYLE =
                new WritableIntPropertyKey();
        public static final WritableIntPropertyKey SECONDARY_TEXT_STYLE =
                new WritableIntPropertyKey();

        static final PropertyKey[] ALL_KEYS = {LABEL, URL, IS_SELECTED, BORDER_COLOR,
                CLICK_LISTENER, BACKGROUND_COLOR, PRIMARY_TEXT_STYLE, SECONDARY_TEXT_STYLE};
    }

    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey FOREGROUND_COLOR = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<OnClickListener> DISMISS_CLICK_CALLBACK =
            new WritableObjectPropertyKey();
    public static final WritableIntPropertyKey SELECTED_ITEM_POSITION =
            new WritableIntPropertyKey();
    // Provider properties
    static final WritableObjectPropertyKey<String> PROVIDER_LABEL =
            new WritableObjectPropertyKey<>();
    static final WritableIntPropertyKey PROVIDER_ICON_RESOURCE = new WritableIntPropertyKey();
    static final WritableObjectPropertyKey<OnClickListener> PROVIDER_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    static final WritableIntPropertyKey PROVIDER_TEXT_STYLE = new WritableIntPropertyKey();

    /**
     * Properties used for the root view. The root view currently contains the RecyclerView
     * and the dismiss button.
     */
    public static final PropertyKey[] ALL_KEYS = {BACKGROUND_COLOR, FOREGROUND_COLOR,
            DISMISS_CLICK_CALLBACK, SELECTED_ITEM_POSITION, PROVIDER_LABEL, PROVIDER_ICON_RESOURCE,
            PROVIDER_CLICK_LISTENER, PROVIDER_TEXT_STYLE};
}
