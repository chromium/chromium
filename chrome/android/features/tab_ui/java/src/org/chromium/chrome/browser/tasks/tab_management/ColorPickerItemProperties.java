// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Model for a color entry in the color picker UI. */
public class ColorPickerItemProperties {
    /** The {@link TabGroupColorId} represented by this entry. */
    public static final ReadableIntPropertyKey COLOR_ID = new ReadableIntPropertyKey();

    /** The {@link ColorPickerType} that this color item corresponds to. */
    public static final ReadableIntPropertyKey COLOR_PICKER_TYPE = new ReadableIntPropertyKey();

    /** An indicator of whether the current tab model is in incognito mode. */
    public static final ReadableBooleanPropertyKey IS_INCOGNITO = new ReadableBooleanPropertyKey();

    /** The function to run when this color item is selected by the user. */
    public static final ReadableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>();

    /** An indicator of whether this color is the currently selected one. */
    public static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();

    /** Creates a model for a color item. */
    public static PropertyModel create(
            int color,
            @ColorPickerType int colorPickerType,
            boolean isIncognito,
            Runnable onClickListener,
            boolean isSelected) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(COLOR_ID, color)
                .with(COLOR_PICKER_TYPE, colorPickerType)
                .with(IS_INCOGNITO, isIncognito)
                .with(ON_CLICK_LISTENER, onClickListener)
                .with(IS_SELECTED, isSelected)
                .build();
    }

    public static final PropertyKey[] ALL_KEYS = {
        COLOR_ID, COLOR_PICKER_TYPE, IS_INCOGNITO, ON_CLICK_LISTENER, IS_SELECTED
    };
}
