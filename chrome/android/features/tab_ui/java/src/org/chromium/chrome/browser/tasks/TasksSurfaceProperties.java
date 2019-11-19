// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.MostVisitedListProperties.IS_VISIBLE;

import android.text.TextWatcher;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of the tasks surface properties. */
public class TasksSurfaceProperties {
    private TasksSurfaceProperties() {}

    public static final PropertyModel.WritableBooleanPropertyKey IS_FAKE_SEARCH_BOX_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_TAB_CAROUSEL_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableBooleanPropertyKey IS_VOICE_RECOGNITION_BUTTON_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> FAKE_SEARCH_BOX_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<View.OnClickListener>();
    public static final PropertyModel
            .WritableObjectPropertyKey<TextWatcher> FAKE_SEARCH_BOX_TEXT_WATCHER =
            new PropertyModel.WritableObjectPropertyKey<TextWatcher>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> MORE_TABS_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<View.OnClickListener>();
    public static final PropertyModel.WritableBooleanPropertyKey MV_TILES_VISIBLE = IS_VISIBLE;
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> VOICE_SEARCH_BUTTON_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<View.OnClickListener>();
    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {IS_FAKE_SEARCH_BOX_VISIBLE,
            IS_INCOGNITO, IS_TAB_CAROUSEL_VISIBLE, IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
            FAKE_SEARCH_BOX_CLICK_LISTENER, FAKE_SEARCH_BOX_TEXT_WATCHER, MORE_TABS_CLICK_LISTENER,
            MV_TILES_VISIBLE, VOICE_SEARCH_BUTTON_CLICK_LISTENER};
}
