// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.support.annotation.DrawableRes;
import android.support.annotation.IdRes;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.RoundedCornerImageView;

/**
 * Container view for omnibox suggestions supplying icon decoration.
 */
class DecoratedSuggestionView extends SimpleHorizontalLayoutView {
    private final RoundedCornerImageView mSuggestionIcon;
    private View mContentView;

    /**
     * Constructs a new suggestion view.
     *
     * @param context The context used to construct the suggestion view.
     * @param background Selectable background resource ID.
     */
    DecoratedSuggestionView(Context context, @DrawableRes int background) {
        super(context);

        setBackgroundResource(background);
        setClickable(true);
        setFocusable(true);

        mSuggestionIcon = new RoundedCornerImageView(getContext());
        mSuggestionIcon.setScaleType(ImageView.ScaleType.FIT_CENTER);

        mSuggestionIcon.setLayoutParams(new LayoutParams(
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_icon_area_size),
                ViewGroup.LayoutParams.WRAP_CONTENT));
        addView(mSuggestionIcon);
    }

    /** Specify content view (suggestion body).  */
    void setContentView(View view) {
        if (mContentView != null) removeView(view);
        mContentView = view;
        mContentView.setLayoutParams(LayoutParams.forDynamicView());
        addView(mContentView);
    }

    /** Returns widget holding suggestion decoration icon.  */
    RoundedCornerImageView getImageView() {
        return mSuggestionIcon;
    }

    @Override
    public boolean isFocused() {
        return super.isFocused() || (isSelected() && !isInTouchMode());
    }

    <ViewType extends View> ViewType findContentView(@IdRes int id) {
        if (mContentView == null) {
            return null;
        }
        return mContentView.findViewById(id);
    }
}
