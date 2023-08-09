// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.url.GURL;

class SearchBoxDataProvider implements LocationBarDataProvider {
    private final @ColorInt int mPrimaryColor;
    private boolean mIsFromQuickActionSearchWidget;
    private Tab mTab;
    private GURL mGurl;

    /**
     * @param context The {@link Context} for accessing colors.
     * @param isFromQuickActionSearchWidget
     */
    SearchBoxDataProvider(Context context) {
        mIsFromQuickActionSearchWidget = false;
        mPrimaryColor = ChromeColors.getPrimaryBackgroundColor(context, isIncognito());
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
        return false;
    }

    @Override
    public boolean isInOverviewAndShowingOmnibox() {
        return false;
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
    public NewTabPageDelegate getNewTabPageDelegate() {
        return NewTabPageDelegate.EMPTY;
    }

    @Override
    public boolean isLoading() {
        return false;
    }

    @Override
    public void addObserver(Observer observer) {}

    @Override
    public void removeObserver(Observer observer) {}

    @Override
    public GURL getCurrentGurl() {
        if (mGurl == null) {
            assert LibraryLoader.getInstance().isInitialized();
            mGurl = new GURL(SearchActivityPreferencesManager.getCurrent().searchEngineUrl);
        }

        return mGurl;
    }

    @Override
    public boolean isOfflinePage() {
        return false;
    }

    @Override
    public int getSecurityLevel() {
        return ConnectionSecurityLevel.NONE;
    }

    @Override
    public int getPageClassification(boolean isFocusedFromFakebox, boolean isPrefetch) {
        if (mIsFromQuickActionSearchWidget) {
            return PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE;
        } else {
            return PageClassification.ANDROID_SEARCH_WIDGET_VALUE;
        }
    }

    @Override
    public int getSecurityIconResource(boolean isTablet) {
        return 0;
    }

    @Override
    public @ColorRes int getSecurityIconColorStateList() {
        return 0;
    }

    @Override
    public int getSecurityIconContentDescriptionResourceId() {
        return 0;
    }

    void setIsFromQuickActionSearchWidget(boolean isFromQuickActionsWidget) {
        mIsFromQuickActionSearchWidget = isFromQuickActionsWidget;
    }
}
