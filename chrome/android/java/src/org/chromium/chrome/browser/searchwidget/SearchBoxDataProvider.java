// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.content.res.Resources;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.security_state.ConnectionSecurityLevel;

class SearchBoxDataProvider implements ToolbarDataProvider {
    private final @ColorInt int mPrimaryColor;
    private Tab mTab;

    /**
     * @param resources The {@link Resources} for accessing colors.
     */
    SearchBoxDataProvider(Resources resources) {
        mPrimaryColor = ChromeColors.getPrimaryBackgroundColor(resources, isIncognito());
    }

    /**
     * Called when native library is loaded and a tab has been initialized.
     * @param tab The tab to use.
     */
    public void onNativeLibraryReady(Tab tab) {
        assert LibraryLoader.getInstance().isInitialized();
        mTab = tab;
    }

    @Override
    public boolean isUsingBrandColor() {
        return false;
    }

    @Override
    public boolean isIncognito() {
        if (mTab == null) return false;
        return mTab.isIncognito();
    }

    @Override
    public boolean isInOverviewAndShowingOmnibox() {
        return false;
    }

    @Override
    public boolean shouldShowLocationBarInOverviewMode() {
        return false;
    }

    @Override
    public Profile getProfile() {
        return mTab != null ? Profile.fromWebContents(mTab.getWebContents()) : null;
    }

    @Override
    public UrlBarData getUrlBarData() {
        return UrlBarData.EMPTY;
    }

    @Override
    public String getTitle() {
        return "";
    }

    @Override
    public Tab getTab() {
        return mTab;
    }

    @Override
    public boolean hasTab() {
        return mTab != null;
    }

    @Override
    public int getPrimaryColor() {
        return mPrimaryColor;
    }

    @Override
    public NewTabPage getNewTabPageForCurrentTab() {
        return null;
    }

    @Override
    public String getCurrentUrl() {
        return SearchWidgetProvider.getDefaultSearchEngineUrl();
    }

    @Override
    public boolean isOfflinePage() {
        return false;
    }

    @Override
    public boolean isPreview() {
        return false;
    }

    @Override
    public int getSecurityLevel() {
        return ConnectionSecurityLevel.NONE;
    }

    @Override
    public int getSecurityIconResource(boolean isTablet) {
        return 0;
    }

    @Override
    public @ColorRes int getSecurityIconColorStateList() {
        return 0;
    }
}
