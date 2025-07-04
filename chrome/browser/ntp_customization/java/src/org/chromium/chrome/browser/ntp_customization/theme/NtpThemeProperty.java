// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import android.util.Pair;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class NtpThemeProperty {
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            LEARN_MORE_BUTTON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<Pair<Integer, Integer>>
            LEADING_ICON_FOR_THEME_COLLECTIONS = new PropertyModel.WritableObjectPropertyKey<>();

    // The key manages the visibility of trailing icon for each section, with the integer
    // representing the section type and the boolean indicating its visibility.
    public static final PropertyModel.WritableObjectPropertyKey<Pair<Integer, Boolean>>
            IS_SECTION_TRAILING_ICON_VISIBLE = new PropertyModel.WritableObjectPropertyKey<>();

    // The key manages the {@link View.OnClickListener} of each section, with the integer
    // representing the section type.
    public static final PropertyModel.WritableObjectPropertyKey<Pair<Integer, View.OnClickListener>>
            SECTION_ON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] THEME_KEYS =
            new PropertyKey[] {
                LEARN_MORE_BUTTON_CLICK_LISTENER,
                IS_SECTION_TRAILING_ICON_VISIBLE,
                SECTION_ON_CLICK_LISTENER,
                LEADING_ICON_FOR_THEME_COLLECTIONS
            };
}
