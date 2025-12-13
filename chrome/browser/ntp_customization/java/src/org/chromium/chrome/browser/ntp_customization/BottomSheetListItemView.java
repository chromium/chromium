// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.widget.TextViewWithLeading;

/** The list item view within a {@link BottomSheetListContainerView} of a bottom sheet. */
@NullMarked
public class BottomSheetListItemView extends LinearLayout {
    private TextViewWithLeading mTitleView;
    private TextViewWithLeading mSubtitleView;
    private ImageView mTrailingIcon;

    public BottomSheetListItemView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.title);
        mSubtitleView = findViewById(R.id.subtitle);
        mTrailingIcon = findViewById(R.id.trailing_icon);
    }

    void setTitle(String text) {
        mTitleView.setText(text);
    }

    /**
     * Sets the content of the subtitle below the title in this list item view. When the given text
     * is null, the visibility of the subtitle view is set to View.GONE.
     *
     * @param text The text to display under the title.
     */
    void setSubtitle(@Nullable String text) {
        if (text == null) {
            mSubtitleView.setVisibility(View.GONE);
        }
        mSubtitleView.setText(text);
    }

    /** Sets the background of this {@link BottomSheetListItemView}. */
    void setBackground(@DrawableRes int resId) {
        this.setBackground(AppCompatResources.getDrawable(getContext(), resId));
    }

    /**
     * Sets the image resource of the trailing icon besides the title and the subtitle. When the
     * given resId is null, the visibility of the icon will be set to View.GONE.
     */
    void setTrailingIcon(@Nullable @DrawableRes Integer resId) {
        if (resId == null) {
            mTrailingIcon.setVisibility(View.GONE);
            return;
        }
        mTrailingIcon.setImageResource(resId);
    }

    /** Sets the content description of the trailing icon besides the title and the subtitle. */
    void setTrailingIconContentDescriptionResId(@StringRes int contentDescription) {
        mTrailingIcon.setContentDescription(getResources().getString(contentDescription));
    }
}
