// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.button;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Holds a {@link Callback<View>} for handling on press events, and delegates everything else to a
 * {@link DisplayButtonData}.
 */
@NullMarked
public class DelegateButtonData implements ActionButtonData {
    private final DisplayButtonData mDelegateButtonData;
    private final @Nullable Callback<View> mOnPress;
    private final @Nullable Callback<View> mOnLongPress;
    private boolean mIsTransparent;
    private boolean mIsToggled;

    /** Builder class for {@link DelegateButtonData}. */
    public static class Builder {
        private final DisplayButtonData mDelegateButtonData;
        private @Nullable Callback<View> mOnPress;
        private @Nullable Callback<View> mOnLongPress;
        private boolean mIsTransparent;
        private boolean mIsToggled;

        /**
         * Creates a builder for a {@link DelegateButtonData}.
         *
         * @param delegateButtonData The {@link DisplayButtonData} representing the button visuals.
         */
        public Builder(DisplayButtonData delegateButtonData) {
            mDelegateButtonData = delegateButtonData;
        }

        /** Sets the {@link Callback<View>} to invoke when the button is pressed. */
        public Builder setOnPress(@Nullable Callback<View> onPress) {
            mOnPress = onPress;
            return this;
        }

        /** Sets the {@link Callback<View>} to invoke when the button is long-pressed. */
        public Builder setOnLongPress(@Nullable Callback<View> onLongPress) {
            mOnLongPress = onLongPress;
            return this;
        }

        /** Sets whether the button should be transparent. */
        public Builder setIsTransparent(boolean isTransparent) {
            mIsTransparent = isTransparent;
            return this;
        }

        /** Sets whether the button should be in a toggled state. */
        public Builder setIsToggled(boolean isToggled) {
            mIsToggled = isToggled;
            return this;
        }

        /** Builds the {@link DelegateButtonData}. */
        public DelegateButtonData build() {
            return new DelegateButtonData(
                    mDelegateButtonData, mOnPress, mOnLongPress, mIsTransparent, mIsToggled);
        }
    }

    private DelegateButtonData(
            DisplayButtonData delegateButtonData,
            @Nullable Callback<View> onPress,
            @Nullable Callback<View> onLongPress,
            boolean isTransparent,
            boolean isToggled) {
        mDelegateButtonData = delegateButtonData;
        mOnPress = onPress;
        mOnLongPress = onLongPress;
        mIsTransparent = isTransparent;
        mIsToggled = isToggled;
    }

    /**
     * Sets whether the button should be transparent.
     *
     * @param isTransparent true if the button should be transparent, false otherwise.
     */
    public void setIsTransparent(boolean isTransparent) {
        mIsTransparent = isTransparent;
    }

    /**
     * Sets whether the button should be in a toggled state.
     *
     * @param isToggled true if the button should be toggled, false otherwise.
     */
    public void setIsToggled(boolean isToggled) {
        mIsToggled = isToggled;
    }

    @Override
    public boolean isTransparent() {
        return mIsTransparent;
    }

    @Override
    public boolean isToggled() {
        return mIsToggled;
    }

    @Override
    public String resolveText(Context context) {
        return mDelegateButtonData.resolveText(context);
    }

    @Override
    public String resolveContentDescription(Context context) {
        return mDelegateButtonData.resolveContentDescription(context);
    }

    @Override
    public Drawable resolveIcon(Context context) {
        return mDelegateButtonData.resolveIcon(context);
    }

    @Override
    public @Nullable Callback<View> getOnPress() {
        return mOnPress;
    }

    @Override
    public @Nullable Callback<View> getOnLongPress() {
        return mOnLongPress;
    }

    @Override
    public boolean buttonDataEquals(Object o) {
        if (this == o) {
            return true;
        }
        if (o instanceof DelegateButtonData that) {
            return mDelegateButtonData.equals(that.mDelegateButtonData)
                    && mIsToggled == that.mIsToggled
                    && mIsTransparent == that.mIsTransparent;
        }
        return false;
    }
}
