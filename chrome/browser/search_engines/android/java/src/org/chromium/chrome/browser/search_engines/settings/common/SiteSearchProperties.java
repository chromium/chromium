// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import android.graphics.Bitmap;
import android.view.View.OnClickListener;

import androidx.annotation.IntDef;
import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
public class SiteSearchProperties {
    @IntDef({ViewType.SEARCH_ENGINE, ViewType.ADD, ViewType.MORE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViewType {
        int SEARCH_ENGINE = 0;
        int ADD = 1;
        int MORE = 2;
    }

    public static final WritableObjectPropertyKey<Adapter> ADAPTER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> SITE_NAME =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> SITE_SHORTCUT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Bitmap> ICON = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<OnClickListener> ON_CLICK =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> TEXT = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Boolean> IS_EXPANDED =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<ListMenuDelegate> MENU_DELEGATE =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        ADAPTER, ICON, IS_EXPANDED, MENU_DELEGATE, ON_CLICK, SITE_NAME, SITE_SHORTCUT, TEXT,
    };
}
