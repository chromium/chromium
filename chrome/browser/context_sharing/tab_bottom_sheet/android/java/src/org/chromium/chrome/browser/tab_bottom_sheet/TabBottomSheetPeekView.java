// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;
import androidx.core.content.ContextCompat;

import com.google.android.material.button.MaterialButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.ui.widget.ChromeImageButton;

/** A {@link RelativeLayout} for the tab bottom sheet peek view. */
@NullMarked
class TabBottomSheetPeekView extends RelativeLayout {
    private TextView mTitleView;
    private TextView mDescriptionView;
    private MaterialButton mActionButton;
    private ChromeImageButton mCloseIcon;

    public TabBottomSheetPeekView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        // Inflate standard child views
        mTitleView = findViewById(R.id.peek_title);
        mDescriptionView = findViewById(R.id.peek_description);
        mActionButton = findViewById(R.id.peek_action_button);
        mCloseIcon = findViewById(R.id.peek_close_button);
    }

    /**
     * Sets the main title text for the task.
     *
     * @param title The string to display.
     */
    public void setTitle(String title) {
        mTitleView.setText(title);
    }

    /**
     * Sets the title text appearance.
     *
     * @param resId The text appearance resource ID.
     */
    public void setTitleTextAppearance(@StyleRes int resId) {
        mTitleView.setTextAppearance(getContext(), resId);
    }

    /**
     * Sets the description text for the current step.
     *
     * @param textResId The string resource ID describing the active step.
     */
    public void setDescriptionText(@StringRes int textResId) {
        if (textResId == Resources.ID_NULL) {
            mDescriptionView.setText(null);
        } else {
            mDescriptionView.setText(textResId);
        }
    }

    /**
     * Sets the description visibility.
     *
     * @param visibility The visibility state.
     */
    public void setDescriptionVisibility(int visibility) {
        mDescriptionView.setVisibility(visibility);
    }

    /**
     * Sets the action button text.
     *
     * @param textResId The action button label resource ID.
     */
    public void setActionButtonText(@StringRes int textResId) {
        if (textResId == Resources.ID_NULL) {
            mActionButton.setText(null);
        } else {
            mActionButton.setText(textResId);
        }
    }

    /**
     * Sets the visibility of the action button.
     *
     * @param visibility The visibility state.
     */
    public void setActionButtonVisibility(int visibility) {
        mActionButton.setVisibility(visibility);
    }

    /**
     * Sets the action button icon drawable.
     *
     * @param resId The icon drawable resource ID.
     */
    public void setActionButtonIcon(@DrawableRes int resId) {
        mActionButton.setIconResource(resId);
    }

    /**
     * Sets the action button background tint list.
     *
     * @param tintResId The background tint color resource ID.
     */
    public void setActionButtonBackgroundTint(@ColorRes int tintResId) {
        if (tintResId == Resources.ID_NULL) {
            mActionButton.setBackgroundTintList(null);
        } else {
            mActionButton.setBackgroundTintList(
                    ColorStateList.valueOf(ContextCompat.getColor(getContext(), tintResId)));
        }
    }

    /**
     * Sets the action button icon tint list.
     *
     * @param tintResId The icon tint color resource ID.
     */
    public void setActionButtonIconTint(@ColorRes int tintResId) {
        if (tintResId == Resources.ID_NULL) {
            mActionButton.setIconTint(null);
        } else {
            mActionButton.setIconTint(ContextCompat.getColorStateList(getContext(), tintResId));
        }
    }

    /**
     * Sets the action button horizontal padding.
     *
     * @param paddingResId The horizontal padding dimension resource ID.
     */
    public void setActionButtonHorizontalPadding(@DimenRes int paddingResId) {
        int padding =
                paddingResId == Resources.ID_NULL
                        ? 0
                        : getContext().getResources().getDimensionPixelSize(paddingResId);
        mActionButton.setPaddingRelative(padding, 0, padding, 0);
    }

    /**
     * Sets the action button content description.
     *
     * @param textResId The content description resource ID.
     */
    public void setActionButtonContentDescription(@StringRes int textResId) {
        if (textResId == Resources.ID_NULL) {
            mActionButton.setContentDescription(null);
        } else {
            mActionButton.setContentDescription(getContext().getString(textResId));
        }
    }

    /**
     * Sets the click listener for the action button.
     *
     * @param listener The callback to be invoked.
     */
    public void setActionButtonClickListener(OnClickListener listener) {
        mActionButton.setOnClickListener(listener);
    }

    /**
     * Sets the click listener for the close button.
     *
     * @param listener The callback to be invoked.
     */
    public void setCloseClickListener(OnClickListener listener) {
        mCloseIcon.setOnClickListener(listener);
    }

    /**
     * Sets the click listener for the peek view.
     *
     * @param listener The callback to be invoked.
     */
    public void setPeekViewClickListener(OnClickListener listener) {
        setOnClickListener(listener);
    }

    public String getStepDescriptionForTesting() {
        return mDescriptionView.getText().toString();
    }
}
