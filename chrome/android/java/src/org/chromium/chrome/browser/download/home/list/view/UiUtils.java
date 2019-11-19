// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.view;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.support.v7.content.res.AppCompatResources;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StyleableRes;

/** A set of helper methods to make interacting with the Android UI easier. */
public final class UiUtils {
    private UiUtils() {}

    /**
     * Loads a {@link Drawable} from an attribute.  Uses {@link AppCompatResources} to support all
     * modern {@link Drawable} types.
     * @return A new {@link Drawable} or {@code null} if the attribute wasn't set.
     */
    public static @Nullable Drawable getDrawable(
            Context context, @Nullable TypedArray attrs, @StyleableRes int attrId) {
        if (attrs == null) return null;

        @DrawableRes
        int resId = attrs.getResourceId(attrId, -1);
        if (resId == -1) return null;
        return UiUtils.getDrawable(context, resId);
    }

    /**
     * Loads a {@link Drawable} from a resource Id.  Uses {@link AppCompatResources} to support all
     * modern {@link Drawable} types.
     * @return A new {@link Drawable}.
     */
    public static Drawable getDrawable(Context context, @DrawableRes int resId) {
        return AppCompatResources.getDrawable(context, resId);
    }
}