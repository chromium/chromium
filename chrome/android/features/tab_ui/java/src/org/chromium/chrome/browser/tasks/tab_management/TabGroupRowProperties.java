// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;

import androidx.core.util.Pair;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** Properties for displaying a single tab group row. */
public class TabGroupRowProperties {
    public static final ReadableObjectPropertyKey<Drawable> START_DRAWABLE =
            new ReadableObjectPropertyKey();
    public static final ReadableIntPropertyKey COLOR_INDEX = new ReadableIntPropertyKey();
    // First is the user title, second is the number of tabs.
    public static final ReadableObjectPropertyKey<Pair<String, Integer>> TITLE_DATA =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Long> CREATION_MILLIS =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Runnable> OPEN_RUNNABLE =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Runnable> DELETE_RUNNABLE =
            new ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        START_DRAWABLE, COLOR_INDEX, TITLE_DATA, CREATION_MILLIS, OPEN_RUNNABLE, DELETE_RUNNABLE
    };
}
