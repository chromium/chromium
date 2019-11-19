// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;

import androidx.annotation.StringRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.TabCountProvider.TabCountObserver;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * The close all tabs button.
 */
class CloseAllTabsButton extends ChromeImageButton
        implements TintObserver, IncognitoStateObserver, TabCountObserver {
    /** A provider that notifies when the theme color changes.*/
    private ThemeColorProvider mThemeColorProvider;

    /** A provider that notifies when incognito mode is entered or exited. */
    private IncognitoStateProvider mIncognitoStateProvider;

    /** A provider that notifies when the number of tabs changes. */
    private TabCountProvider mTabCountProvider;

    /** Whether the close all tabs button should be enabled. */
    private boolean mIsEnabled = true;

    public CloseAllTabsButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    void destroy() {
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeTintObserver(this);
            mThemeColorProvider = null;
        }
        if (mIncognitoStateProvider != null) {
            mIncognitoStateProvider.removeObserver((IncognitoStateObserver) this);
            mIncognitoStateProvider = null;
        }
        if (mTabCountProvider != null) {
            mTabCountProvider.removeObserver(this);
            mTabCountProvider = null;
        }
    }

    void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(this);
    }

    @Override
    public void onTintChanged(ColorStateList tint, boolean useLight) {
        ApiCompatibilityUtils.setImageTintList(this, tint);
    }

    void setIncognitoStateProvider(IncognitoStateProvider incognitoStateProvider) {
        mIncognitoStateProvider = incognitoStateProvider;
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(this);
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        @StringRes
        int resId = isIncognito ? R.string.accessibility_toolbar_btn_close_all_incognito_tabs
                                : R.string.accessibility_toolbar_btn_close_all_tabs;
        setContentDescription(getResources().getText(resId));
    }

    void setTabCountProvider(TabCountProvider provider) {
        mTabCountProvider = provider;
        mTabCountProvider.addObserver(this);
    }

    @Override
    public void onTabCountChanged(int tabCount, boolean isIncognito) {
        final boolean shouldBeEnabled = tabCount > 0;
        if (shouldBeEnabled == mIsEnabled) return;

        mIsEnabled = shouldBeEnabled;
        setEnabled(mIsEnabled);
    }
}
