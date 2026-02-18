// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import android.graphics.Bitmap;
import android.view.View.OnClickListener;

import androidx.annotation.IntDef;
import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
public class CustomSearchEngineProperties {
    @IntDef({
        CustomSearchEngineRecyclerViewItems.DEFAULT,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface CustomSearchEngineRecyclerViewItems {
        int DEFAULT = 0;
    }

    static final WritableObjectPropertyKey<Adapter> ADAPTER = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> NAME = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> URL = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Bitmap> ICON = new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<OnClickListener> MENU_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ADAPTER, CLICK_LISTENER, ICON, MENU_CLICK_LISTENER, NAME, URL,
            };
}
