// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.annotation.StringRes;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.AppCompatImageButton;
import android.util.AttributeSet;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Button for creating new tabs.
 */
public class NewTabButton extends AppCompatImageButton {
    private final ColorStateList mLightModeTint;
    private final ColorStateList mDarkModeTint;
    private boolean mIsIncognito;
    private boolean mIsNativeReady;

    /**
     * Constructor for inflating from XML.
     */
    public NewTabButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        mIsIncognito = false;
        mLightModeTint =
                AppCompatResources.getColorStateList(getContext(), R.color.light_mode_tint);
        mDarkModeTint = AppCompatResources.getColorStateList(getContext(), R.color.dark_mode_tint);
        setImageDrawable(VectorDrawableCompat.create(
                getContext().getResources(), R.drawable.new_tab_icon, getContext().getTheme()));
        updateDrawableTint();
    }

    /**
     * Called to finish initializing the NewTabButton. Must be called after native initialization
     * is finished.
     */
    public void postNativeInitialization() {
        mIsNativeReady = true;
        updateDrawableTint();
    }

    /**
     * Updates the visual state based on whether incognito or normal tabs are being created.
     * @param incognito Whether the button is now used for creating incognito tabs.
     */
    public void setIsIncognito(boolean incognito) {
        if (mIsIncognito == incognito) return;
        mIsIncognito = incognito;

        @StringRes
        int resId;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_STRINGS)) {
            resId = mIsIncognito ? R.string.accessibility_toolbar_btn_new_private_tab
                                 : R.string.accessibility_toolbar_btn_new_tab;
        } else {
            resId = mIsIncognito ? R.string.accessibility_toolbar_btn_new_incognito_tab
                                 : R.string.accessibility_toolbar_btn_new_tab;
        }
        setContentDescription(getResources().getText(resId));

        updateDrawableTint();
        invalidate();
    }

    /** Called when accessibility status is changed. */
    public void onAccessibilityStatusChanged() {
        updateDrawableTint();
    }

    /** Update the tint for the icon drawable for Chrome Modern. */
    private void updateDrawableTint() {
        final boolean shouldUseLightMode =
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())
                || (mIsNativeReady
                           && (DeviceClassManager.enableAccessibilityLayout()
                                      || ChromeFeatureList.isEnabled(
                                                 ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID))
                           && mIsIncognito);
        ApiCompatibilityUtils.setImageTintList(
                this, shouldUseLightMode ? mLightModeTint : mDarkModeTint);
    }
}
