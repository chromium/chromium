// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.google.android.material.button.MaterialButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
    public void setTitleTextAppearance(int resId) {
        mTitleView.setTextAppearance(getContext(), resId);
    }

    /**
     * Sets the description text for the current step.
     *
     * @param text The string describing the active step.
     */
    public void setDescriptionText(String text) {
        mDescriptionView.setText(text);
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
     * @param text The action button label.
     */
    public void setActionButtonText(@Nullable String text) {
        mActionButton.setText(text);
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
    public void setActionButtonIcon(int resId) {
        mActionButton.setIconResource(resId);
    }

    /**
     * Sets the action button background tint list.
     *
     * @param tint The ColorStateList tint list.
     */
    public void setActionButtonBackgroundTint(@Nullable ColorStateList tint) {
        mActionButton.setBackgroundTintList(tint);
    }

    /**
     * Sets the action button icon tint list.
     *
     * @param tint The ColorStateList tint list.
     */
    public void setActionButtonIconTint(@Nullable ColorStateList tint) {
        mActionButton.setIconTint(tint);
    }

    /**
     * Sets the action button horizontal padding.
     *
     * @param padding The horizontal padding value in pixels.
     */
    public void setActionButtonHorizontalPadding(int padding) {
        mActionButton.setPaddingRelative(padding, 0, padding, 0);
    }

    /**
     * Sets the action button content description.
     *
     * @param contentDescription The content description for accessibility.
     */
    public void setActionButtonContentDescription(@Nullable String contentDescription) {
        mActionButton.setContentDescription(contentDescription);
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
