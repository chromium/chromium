// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import java.util.Objects;

/** Resolves data by reading from Android resources on demand. */
public class ResourceButtonData implements DisplayButtonData {
    private final @StringRes int mTextRes;
    private final @StringRes int mContentDescriptionRes;
    private final @DrawableRes int mIconRes;

    /**
     * Stores resource ids until resolution time.
     *
     * @param textRes The text resource to resolve.
     * @param contentDescriptionRes The content description resource to resolve.
     * @param drawableRes The drawable resource to resolve.
     */
    public ResourceButtonData(
            @StringRes int textRes,
            @StringRes int contentDescriptionRes,
            @DrawableRes int iconRes) {
        mTextRes = textRes;
        mContentDescriptionRes = contentDescriptionRes;
        mIconRes = iconRes;
    }

    @Override
    public String resolveText(Context context) {
        return context.getString(mTextRes);
    }

    @Override
    public String resolveContentDescription(Context context) {
        return context.getString(mContentDescriptionRes);
    }

    @Override
    public Drawable resolveIcon(Context context) {
        return AppCompatResources.getDrawable(context, mIconRes);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mTextRes, mContentDescriptionRes, mIconRes);
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) {
            return true;
        }
        if (o instanceof ResourceButtonData that) {
            return mTextRes == that.mTextRes
                    && mContentDescriptionRes == that.mContentDescriptionRes
                    && mIconRes == that.mIconRes;
        }
        return false;
    }
}
