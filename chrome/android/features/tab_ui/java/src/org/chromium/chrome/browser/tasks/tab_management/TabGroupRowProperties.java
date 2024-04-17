// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;

import androidx.core.util.Consumer;
import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for displaying a single tab group row. */
public class TabGroupRowProperties {
    @FunctionalInterface
    public interface AsyncDrawable extends Consumer<Callback<Drawable>> {}

    public static final WritableObjectPropertyKey<AsyncDrawable> ASYNC_FAVICON_TOP_LEFT =
            new WritableObjectPropertyKey();
    public static final WritableObjectPropertyKey<AsyncDrawable> ASYNC_FAVICON_TOP_RIGHT =
            new WritableObjectPropertyKey();
    public static final WritableObjectPropertyKey<AsyncDrawable> ASYNC_FAVICON_BOTTOM_LEFT =
            new WritableObjectPropertyKey();
    // These two properties are exclusive, only one can be shown at a time. The favicon takes
    // precedence, only when the favicon is null will the count be used.
    public static final WritableObjectPropertyKey<AsyncDrawable> ASYNC_FAVICON_BOTTOM_RIGHT =
            new WritableObjectPropertyKey();
    public static final WritableObjectPropertyKey<Integer> PLUS_COUNT =
            new WritableObjectPropertyKey();

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
        ASYNC_FAVICON_TOP_LEFT,
        ASYNC_FAVICON_TOP_RIGHT,
        ASYNC_FAVICON_BOTTOM_LEFT,
        ASYNC_FAVICON_BOTTOM_RIGHT,
        PLUS_COUNT,
        COLOR_INDEX,
        TITLE_DATA,
        CREATION_MILLIS,
        OPEN_RUNNABLE,
        DELETE_RUNNABLE
    };
}
