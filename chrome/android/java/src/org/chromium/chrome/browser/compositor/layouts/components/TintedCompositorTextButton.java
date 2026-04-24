// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.components;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;

/**
 * {@link TintedCompositorTextButton} is a {@link TintedCompositorButton} that can also display text
 * and an internal dismiss button.
 */
@NullMarked
public class TintedCompositorTextButton extends TintedCompositorButton {
    private @Nullable String mText;
    private int mTextResourceId;
    private final @Nullable TintedCompositorButton mDismissButton;
    private boolean mDismissButtonClicked;

    public TintedCompositorTextButton(
            Context context,
            boolean incognito,
            @ButtonType int type,
            @Nullable StripLayoutView parentView,
            float width,
            float height,
            @Nullable TooltipHandler tooltipHandler,
            StripLayoutViewOnClickHandler clickHandler,
            StripLayoutViewOnKeyboardFocusHandler keyboardFocusHandler,
            @DrawableRes int resource,
            float clickSlopDp,
            boolean hasLongClickAction,
            @Nullable TintedCompositorButton dismissButton) {
        super(
                context,
                incognito,
                type,
                parentView,
                width,
                height,
                tooltipHandler,
                clickHandler,
                keyboardFocusHandler,
                resource,
                Resources.ID_NULL,
                clickSlopDp,
                hasLongClickAction);
        mDismissButton = dismissButton;
    }

    /**
     * @return The dismiss sub-button nested inside this text button.
     */
    public @Nullable TintedCompositorButton getDismissButton() {
        return mDismissButton;
    }

    @Override
    public boolean checkClickedOrHovered(float x, float y) {
        if (mDismissButton != null && mDismissButton.checkClickedOrHovered(x, y)) {
            return true;
        }

        return super.checkClickedOrHovered(x, y);
    }

    @Override
    public boolean onDown(float x, float y, int buttons) {
        if (mDismissButton != null && mDismissButton.onDown(x, y, buttons)) {
            return true;
        }
        return super.onDown(x, y, buttons);
    }

    @Override
    public boolean click(float x, float y, int buttons) {
        mDismissButtonClicked = false;
        if (mDismissButton != null && mDismissButton.click(x, y, buttons)) {
            mDismissButtonClicked = true;
            return true;
        }
        return super.click(x, y, buttons);
    }

    @Override
    public boolean drag(float x, float y) {
        if (mDismissButton != null && mDismissButton.checkClickedOrHovered(x, y)) {
            return mDismissButton.drag(x, y);
        }
        return super.drag(x, y);
    }

    @Override
    public boolean onUpOrCancel() {
        boolean superState = super.onUpOrCancel();
        boolean childState = false;
        if (mDismissButton != null) {
            childState = mDismissButton.onUpOrCancel();
        }
        return superState || childState;
    }

    @Override
    public void handleClick(long time, int buttons, int modifiers) {
        if (mDismissButton != null && mDismissButtonClicked) {
            mDismissButtonClicked = false;
            mDismissButton.handleClick(time, buttons, modifiers);
        } else {
            super.handleClick(time, buttons, modifiers);
        }
    }

    /**
     * @param text The text to be displayed on the button.
     */
    public void setText(@Nullable String text) {
        mText = text;
    }

    /**
     * @return The text displayed on the button.
     */
    public @Nullable String getText() {
        return mText;
    }

    /**
     * @param textResourceId The resource ID for the generated text bitmap.
     */
    public void setTextResourceId(int textResourceId) {
        mTextResourceId = textResourceId;
    }

    /**
     * @return The resource ID for the generated text bitmap.
     */
    public int getTextResourceId() {
        return mTextResourceId;
    }
}
