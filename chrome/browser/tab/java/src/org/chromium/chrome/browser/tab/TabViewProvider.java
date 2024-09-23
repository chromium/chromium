// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * An interface that provides a {@link View} to be shown in a {@link Tab}.
 * Refer to the Javadoc on {@link TabViewManager} to learn how to add a new {@link TabViewProvider}
 * to a {@link Tab}.
 */
public interface TabViewProvider {
    /**
     * Represents each {@link TabViewProvider} implementer. Please note that the integer values
     * bear no ordering or prioritization meaning.
     */
    @IntDef({Type.SUSPENDED_TAB, Type.SAD_TAB, Type.PAINT_PREVIEW, Type.NEW_DOWNLOAD_TAB})
    @Retention(RetentionPolicy.SOURCE)
    @interface Type {
        int SUSPENDED_TAB = 0;
        int SAD_TAB = 1;
        int PAINT_PREVIEW = 2;
        int NEW_DOWNLOAD_TAB = 3;
    }

    /**
     * @return The {@link Type} associated with this {@link TabViewProvider}.
     */
    @Type
    int getTabViewProviderType();

    /**
     * @return The {@link View} that {@link Tab} is supposed to show.
     */
    View getView();

    /** Called when the {@link View} provided by {@link #getView()} is provided to {@link Tab}. */
    default void onShown() {}

    /** Called when the {@link View} provided by {@link #getView()} is removed from {@link Tab}. */
    default void onHidden() {}

    /**
     * @return The background color for the content to show.
     */
    @ColorInt
    int getBackgroundColor(Context context);
}
