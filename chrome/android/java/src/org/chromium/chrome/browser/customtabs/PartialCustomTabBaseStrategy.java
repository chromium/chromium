// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;

import androidx.annotation.IntDef;
import androidx.annotation.Px;

import org.chromium.chrome.browser.fullscreen.FullscreenManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Base class for PCCT size strategies implementations.
 */
public abstract class PartialCustomTabBaseStrategy
        extends CustomTabHeightStrategy implements FullscreenManager.Observer {
    protected final Activity mActivity;
    protected final @Px int mUnclampedInitialHeight;
    protected final boolean mIsFixedHeight;
    protected final OnResizedCallback mOnResizedCallback;
    protected final FullscreenManager mFullscreenManager;
    protected boolean mIsTablet;
    protected final boolean mInteractWithBackground;

    protected final PartialCustomTabVersionCompat mVersionCompat;
    protected @Px int mDisplayHeight;
    protected @Px int mDisplayWidth;

    @IntDef({PartialCustomTabType.NONE, PartialCustomTabType.BOTTOM_SHEET,
            PartialCustomTabType.SIDE_SHEET, PartialCustomTabType.FULL_SIZE})
    @Retention(RetentionPolicy.SOURCE)
    @interface PartialCustomTabType {
        int NONE = 0;
        int BOTTOM_SHEET = 1;
        int SIDE_SHEET = 2;
        int FULL_SIZE = 3;
    }

    public PartialCustomTabBaseStrategy(Activity activity, @Px int initialHeight,
            boolean isFixedHeight, OnResizedCallback onResizedCallback,
            FullscreenManager fullscreenManager, boolean isTablet, boolean interactWithBackground) {
        mActivity = activity;
        mUnclampedInitialHeight = initialHeight;
        mIsFixedHeight = isFixedHeight;
        mOnResizedCallback = onResizedCallback;
        mFullscreenManager = fullscreenManager;
        mIsTablet = isTablet;
        mInteractWithBackground = interactWithBackground;

        mVersionCompat = PartialCustomTabVersionCompat.create(mActivity, this::updatePosition);
        mDisplayHeight = mVersionCompat.getDisplayHeight();
        mDisplayWidth = mVersionCompat.getDisplayWidth();
    }

    @PartialCustomTabType
    public abstract int getStrategyType();

    public abstract void onShowSoftInput(Runnable softKeyboardRunnable);

    @Override
    public void destroy() {
        mFullscreenManager.removeObserver(this);
    }

    protected abstract void updatePosition();
}
