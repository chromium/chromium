// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.style.DynamicDrawableSpan;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.chrome.tab_ui.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * An implementation of {@link DynamicDrawableSpan} for displaying rating stars in a {@link
 * Spannable}.
 */
public class RatingStarSpan extends DynamicDrawableSpan {
    @IntDef({RatingStarType.OUTLINE, RatingStarType.HALF, RatingStarType.FULL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RatingStarType {
        int OUTLINE = 0;
        int HALF = 1;
        int FULL = 2;
    }

    private final Context mContext;
    private final @RatingStarType int mType;

    public RatingStarSpan(Context context, @RatingStarType int type) {
        mContext = context;
        mType = type;
    }

    @Override
    public Drawable getDrawable() {
        Drawable drawable =
                ResourcesCompat.getDrawable(
                        mContext.getResources(), getResourceId(mType), mContext.getTheme());
        drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());
        return drawable;
    }

    private @DrawableRes int getResourceId(@RatingStarType int type) {
        switch (type) {
            case RatingStarType.OUTLINE:
                return R.drawable.ic_rating_star_outline;
            case RatingStarType.HALF:
                return R.drawable.ic_rating_star_half;
            case RatingStarType.FULL:
                return R.drawable.ic_rating_star_full;
        }
        throw new IllegalArgumentException("RatingStarType value is invalid.");
    }
}
