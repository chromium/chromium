// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.support.annotation.ColorInt;
import android.support.annotation.ColorRes;
import android.support.annotation.DrawableRes;
import android.support.v4.util.ObjectsCompat;
import android.support.v7.content.res.AppCompatResources;

/** Represents graphical decoration for the suggestion components. */
public class SuggestionDrawableState {
    /** Embedded drawable object. */
    public final Drawable drawable;
    /** Whether supplied drawable can be tinted */
    public final boolean allowTint;
    /** Whether drawable should be rounded. */
    public final boolean useRoundedCorners;
    /** Whether drawable should be displayed as large. */
    public final boolean isLarge;

    public static final class Builder {
        private Drawable mDrawable;
        private boolean mAllowTint;
        private boolean mUseRoundedCorners;
        private boolean mIsLarge;

        /**
         * Create new Builder object.
         *
         * @param cxt Current context.
         */
        private Builder(Drawable drawable) {
            assert drawable != null : "SuggestionDrawableState needs a Drawable object";
            mDrawable = drawable;
        }

        /**
         * Associate Bitmap with built SuggestionDrawableState object.
         *
         * @param bitmap Bitmap to use.
         */
        public static Builder forBitmap(Bitmap bitmap) {
            return new Builder(new BitmapDrawable(bitmap));
        }

        /**
         * Associate Color with built SuggestionDrawableState object.
         *
         * @param color Color to use.
         */
        public static Builder forColor(@ColorInt int color) {
            return new Builder(new ColorDrawable(color));
        }

        /**
         * Associate Color with built SuggestionDrawableState object.
         *
         * @param ctx Current context.
         * @param colorRes Color resource to use.
         */
        public static Builder forColorRes(Context ctx, @ColorRes int colorRes) {
            return new Builder(new ColorDrawable(ctx.getResources().getColor(colorRes)));
        }

        /**
         * Associate Drawable with built SuggestionDrawableState object.
         *
         * @param ctx Current context.
         * @param res Drawable resource to use.
         */
        public static Builder forDrawableRes(Context ctx, @DrawableRes int res) {
            return new Builder(AppCompatResources.getDrawable(ctx, res));
        }

        /**
         * Create new SuggestionDrawableState representing a supplied Drawable object.
         *
         * @param drawable Drawable object to use.
         */
        public static Builder forDrawable(Drawable d) {
            return new Builder(d);
        }

        /**
         * Specify whether built object should be rounded.
         *
         * @param useRoundedCorners true, if image should be rounded.
         */
        public Builder setUseRoundedCorners(boolean useRoundedCorners) {
            mUseRoundedCorners = useRoundedCorners;
            return this;
        }

        /**
         * Specify whether build object should receive tint.
         *
         * @param allowTint true, if built drawable state should be tinted to reflect theme.
         */
        public Builder setAllowTint(boolean allowTint) {
            mAllowTint = allowTint;
            return this;
        }

        /**
         * Specify whether build object should be presented as small (24dp) or large (36dp).
         *
         * @param isLarge true, if drawable should be shown large (36dp), otherwise 24dp.
         */
        public Builder setLarge(boolean isLarge) {
            mIsLarge = isLarge;
            return this;
        }

        /**
         * Build SuggestionDrawableState object.
         */
        public SuggestionDrawableState build() {
            return new SuggestionDrawableState(mDrawable, mUseRoundedCorners, mIsLarge, mAllowTint);
        }
    };

    private SuggestionDrawableState(
            Drawable drawable, boolean useRoundedCorners, boolean isLarge, boolean allowTint) {
        this.drawable = drawable;
        this.useRoundedCorners = useRoundedCorners;
        this.isLarge = isLarge;
        this.allowTint = allowTint;
    }

    @Override
    public boolean equals(Object object) {
        if (this == object) return true;
        if (!(object instanceof SuggestionDrawableState)) return false;
        SuggestionDrawableState other = (SuggestionDrawableState) object;

        return isLarge == other.isLarge && useRoundedCorners == other.useRoundedCorners
                && allowTint == other.allowTint && ObjectsCompat.equals(drawable, other.drawable);
    }
};
