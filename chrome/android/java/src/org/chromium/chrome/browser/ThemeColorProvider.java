// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.toolbar.ToolbarColors;
import org.chromium.chrome.browser.util.ColorUtils;

/**
 * An abstract class that provides the current theme color.
 */
public abstract class ThemeColorProvider {
    /**
     * An interface to be notified about changes to the theme color.
     */
    public interface ThemeColorObserver {
        /**
         * @param color The new color the observer should use.
         * @param shouldAnimate Whether the change of color should be animated.
         */
        void onThemeColorChanged(int color, boolean shouldAnimate);
    }

    /**
     * An interface to be notified about changes to the tint.
     */
    public interface TintObserver {
        /**
         * @param tint The new tint the observer should use.
         * @param useLight Whether the observer should use light mode.
         */
        void onTintChanged(ColorStateList tint, boolean useLight);
    }

    /** Light mode tint (used when color is dark). */
    private final ColorStateList mLightModeTint;

    /** Dark mode tint (used when color is light). */
    private final ColorStateList mDarkModeTint;

    /** Current primary color. */
    private int mPrimaryColor;

    /**
     * Whether should use light tint (corresponds to dark color). If null, the state is not
     * initialized.
     */
    private @Nullable Boolean mUseLightTint;

    /** List of {@link ThemeColorObserver}s. These are used to broadcast events to listeners. */
    private final ObserverList<ThemeColorObserver> mThemeColorObservers;

    /** List of {@link TintObserver}s. These are used to broadcast events to listeners. */
    private final ObserverList<TintObserver> mTintObservers;

    /**
     * @param context The {@link Context} that is used to retrieve color related resources.
     */
    public ThemeColorProvider(Context context) {
        mThemeColorObservers = new ObserverList<ThemeColorObserver>();
        mTintObservers = new ObserverList<TintObserver>();
        mLightModeTint = ToolbarColors.getThemedToolbarIconTint(context, true);
        mDarkModeTint = ToolbarColors.getThemedToolbarIconTint(context, false);
    }

    /**
     * @param observer Adds a {@link ThemeColorObserver} that will be notified when the theme color
     *                 changes. This method does not trigger the observer.
     */
    public void addThemeColorObserver(ThemeColorObserver observer) {
        mThemeColorObservers.addObserver(observer);
    }

    /**
     * @param observer Removes the observer so it no longer receives theme color changes.
     */
    public void removeThemeColorObserver(ThemeColorObserver observer) {
        mThemeColorObservers.removeObserver(observer);
    }

    /**
     * @param observer Adds a {@link TintObserver} that will be notified when the tint changes. This
     *                 method does not trigger the observer.
     */
    public void addTintObserver(TintObserver observer) {
        mTintObservers.addObserver(observer);
    }

    /**
     * @param observer Removes the observer so it no longer receives tint changes.
     */
    public void removeTintObserver(TintObserver observer) {
        mTintObservers.removeObserver(observer);
    }

    /**
     * @return The current theme color of this provider.
     */
    @ColorInt
    public int getThemeColor() {
        return mPrimaryColor;
    }

    /**
     * @return The current tint of this provider.
     */
    public ColorStateList getTint() {
        return useLight() ? mLightModeTint : mDarkModeTint;
    }

    /**
     * @return Whether or not this provider is using light tints.
     */
    public boolean useLight() {
        return mUseLightTint != null ? mUseLightTint : false;
    }

    /**
     * Clears out the observer lists.
     */
    public void destroy() {
        mThemeColorObservers.clear();
        mTintObservers.clear();
    }

    protected void updatePrimaryColor(int color, boolean shouldAnimate) {
        if (mPrimaryColor == color) return;
        mPrimaryColor = color;
        for (ThemeColorObserver observer : mThemeColorObservers) {
            observer.onThemeColorChanged(color, shouldAnimate);
        }
        updateTint();
    }

    private void updateTint() {
        final boolean useLight = ColorUtils.shouldUseLightForegroundOnBackground(mPrimaryColor);
        if (mUseLightTint != null && useLight == mUseLightTint) return;
        mUseLightTint = useLight;
        final ColorStateList tint = useLight ? mLightModeTint : mDarkModeTint;
        for (TintObserver observer : mTintObservers) {
            observer.onTintChanged(tint, useLight);
        }
    }
}
