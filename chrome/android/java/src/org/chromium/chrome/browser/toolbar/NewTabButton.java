// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.Toast;

/**
 * Button for creating new tabs.
 */
public class NewTabButton
        extends ChromeImageButton implements IncognitoStateObserver, View.OnLongClickListener {
    private final ColorStateList mLightModeTint;
    private final ColorStateList mDarkModeTint;
    private boolean mIsIncognito;
    private IncognitoStateProvider mIncognitoStateProvider;

    /**
     * Constructor for inflating from XML.
     */
    public NewTabButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        mIsIncognito = false;
        mLightModeTint =
                AppCompatResources.getColorStateList(getContext(), R.color.tint_on_dark_bg);
        mDarkModeTint =
                AppCompatResources.getColorStateList(getContext(), R.color.standard_mode_tint);
        setImageDrawable(VectorDrawableCompat.create(
                getContext().getResources(), R.drawable.new_tab_icon, getContext().getTheme()));
        updateDrawableTint();
        setOnLongClickListener(this);
    }

    @Override
    public boolean onLongClick(View v) {
        CharSequence description = getResources().getString(mIsIncognito
                        ? org.chromium.chrome.R.string.button_new_incognito_tab
                        : org.chromium.chrome.R.string.button_new_tab);
        return Toast.showAnchoredToast(getContext(), v, description);
    }

    public void setIncognitoStateProvider(IncognitoStateProvider incognitoStateProvider) {
        mIncognitoStateProvider = incognitoStateProvider;
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(this);
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        if (mIsIncognito == isIncognito) return;
        mIsIncognito = isIncognito;

        @StringRes
        int resId = mIsIncognito ? R.string.accessibility_toolbar_btn_new_incognito_tab
                                 : R.string.accessibility_toolbar_btn_new_tab;
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
                || ((DeviceClassManager.enableAccessibilityLayout()
                            || ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)
                            || FeatureUtilities.isGridTabSwitcherEnabled())
                        && mIsIncognito);
        ApiCompatibilityUtils.setImageTintList(
                this, shouldUseLightMode ? mLightModeTint : mDarkModeTint);
    }

    /**
     * Clean up any state when the new tab button is destroyed.
     */
    public void destroy() {
        if (mIncognitoStateProvider != null) {
            mIncognitoStateProvider.removeObserver(this);
            mIncognitoStateProvider = null;
        }
    }
}
