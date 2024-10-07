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
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.url.GURL;

class SearchBoxDataProvider implements LocationBarDataProvider {
    private /* PageClassification */ int mPageClassification;
    private @ColorInt int mPrimaryColor;
    private Tab mTab;
    private GURL mGurl;
    private boolean mIsIncognito;

    /**
     * Called when native library is loaded and a tab has been initialized.
     *
     * @param tab The tab to use.
     */
    public void onNativeLibraryReady(Tab tab) {
        mTab = tab;
    }

    /**
     * Initialize this instance of the SearchBoxDataProvider.
     *
     * <p>Note: this is called only once during the lifetime of the SearchActivity, and is not
     * invoked when SearchActivity receives a new Intent.
     *
     * @param context current context
     */
    /* package */ void initialize(Context context, boolean isIncognito) {
        mPrimaryColor = ChromeColors.getPrimaryBackgroundColor(context, isIncognito);
        mIsIncognito = isIncognito;
    }

    @Override
    public boolean isUsingBrandColor() {
        return false;
    }

    @Override
    public boolean isIncognito() {
        return mIsIncognito;
    }

    @Override
    public boolean isIncognitoBranded() {
        return mIsIncognito;
    }

    @Override
    public boolean isOffTheRecord() {
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
        if (GURL.isEmptyOrInvalid(mGurl)) {
            assert LibraryLoader.getInstance().isInitialized();
            mGurl = SearchActivityPreferencesManager.getCurrent().searchEngineUrl;
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
    public int getPageClassification(boolean isPrefetch) {
        return mPageClassification;
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

    void setPageClassification(int pageClassification) {
        mPageClassification = pageClassification;
    }

    void setCurrentUrl(GURL url) {
        mGurl = url;
    }
}
