// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Model for a color entry in the color picker UI. */
public class ColorPickerItemProperties {
    /** The different item types in the RecyclerView on the color picker. */
    @IntDef({ColorItemType.DEFAULT_ITEM})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ColorItemType {
        /** The default item type. */
        int DEFAULT_ITEM = 1;
    }

    /** The {@link TabGroupColorId} represented by this entry. */
    public static final ReadableIntPropertyKey COLOR_ID = new ReadableIntPropertyKey();

    /** An indicator of whether this color is the currently selected one. */
    public static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();

    /** The function to run when this color item is selected by the user. */
    public static final ReadableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>();

    /** Creates a model for a color item. */
    public static PropertyModel create(int color, boolean isSelected, Runnable onClickListener) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(COLOR_ID, color)
                .with(IS_SELECTED, isSelected)
                .with(ON_CLICK_LISTENER, onClickListener)
                .build();
    }

    public static final PropertyKey[] ALL_KEYS = {COLOR_ID, IS_SELECTED, ON_CLICK_LISTENER};
}
