// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import java.util.Objects;

/** Resolves a non-resource drawable's button data. */
public class DrawableButtonData implements DisplayButtonData {
    // The content description should be of type @StringRes, with the exception of use from the
    // TabSwitcherDrawable which will use a @PluralsRes type.
    protected final int mContentDescriptionRes;
    private final @StringRes int mTextRes;
    private final Drawable mDrawable;

    /**
     * @param textRes The text resource to resolve.
     * @param contentDescriptionRes The content description resource to resolve.
     * @param drawable The non-resource {@link Drawable} to display.
     */
    public DrawableButtonData(
            @StringRes int textRes, int contentDescriptionRes, Drawable drawable) {
        mTextRes = textRes;
        mContentDescriptionRes = contentDescriptionRes;
        mDrawable = drawable;
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
        return mDrawable;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mTextRes, mContentDescriptionRes, mDrawable);
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) {
            return true;
        }
        if (o instanceof DrawableButtonData that) {
            return mTextRes == that.mTextRes
                    && mContentDescriptionRes == that.mContentDescriptionRes
                    && mDrawable.equals(that.mDrawable);
        }
        return false;
    }
}
