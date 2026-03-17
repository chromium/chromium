// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Holds a {@link Runnable} for handling on press, and delegates everything else to a {@link
 * DisplayButtonData}.
 */
@NullMarked
public class DelegateButtonData implements ActionButtonData {
    private final DisplayButtonData mDelegateButtonData;
    private final @Nullable Runnable mOnPress;
    private final @Nullable Runnable mOnLongPress;
    private boolean mIsTransparent;
    private boolean mIsToggled;

    /**
     * Stores parameters until resolution time. Never invokes {@link Runnable} itself.
     *
     * @param delegateButtonData The {@link DisplayButtonData} representing the button visuals.
     * @param onPress The runnable to invoke when the button is pressed. A null value will disable
     *     the button.
     */
    public DelegateButtonData(DisplayButtonData delegateButtonData, @Nullable Runnable onPress) {
        this(delegateButtonData, onPress, null);
    }

    /**
     * Stores parameters until resolution time. Never invokes {@link Runnable}s itself.
     *
     * @param delegateButtonData The {@link DisplayButtonData} representing the button visuals.
     * @param onPress The runnable to invoke when the button is pressed. A null value will disable
     *     the button.
     * @param onLongPress The runnable to invoke when the button is long-pressed.
     */
    public DelegateButtonData(
            DisplayButtonData delegateButtonData,
            @Nullable Runnable onPress,
            @Nullable Runnable onLongPress) {
        mDelegateButtonData = delegateButtonData;
        mOnPress = onPress;
        mOnLongPress = onLongPress;
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
    public @Nullable Runnable getOnPressRunnable() {
        return mOnPress;
    }

    @Override
    public @Nullable Runnable getOnLongPressRunnable() {
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
