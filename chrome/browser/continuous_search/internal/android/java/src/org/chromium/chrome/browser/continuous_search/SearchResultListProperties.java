// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Contains model properties for the UI component of Continuous Search Navigation.
 */
public class SearchResultListProperties {
    @IntDef({ListItemType.GROUP_LABEL, ListItemType.SEARCH_RESULT, ListItemType.AD})
    @Retention(RetentionPolicy.SOURCE)

    public @interface ListItemType {
        int GROUP_LABEL = 0;
        int SEARCH_RESULT = 1;
        int AD = 2;
    }

    public static final WritableObjectPropertyKey<String> LABEL = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<GURL> URL = new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {LABEL, URL, IS_SELECTED, CLICK_LISTENER};
}
