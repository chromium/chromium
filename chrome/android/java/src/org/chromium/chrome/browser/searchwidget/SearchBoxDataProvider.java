// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.security_state.ConnectionMaliciousContentStatus;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.url.GURL;

@NullMarked
class SearchBoxDataProvider implements LocationBarDataProvider {
    private final ObservableSupplier<@ControlsPosition Integer> mToolbarPosition =
            new ObservableSupplierImpl(ControlsPosition.TOP);
    private /* PageClassification */ int mPageClassification;
    private @ColorInt int mPrimaryColor;
    private @Nullable GURL mGurl;
    private boolean mIsIncognito;

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
    public @Nullable Tab getTab() {
        return null;
    }

    @Override
    public boolean hasTab() {
        return false;
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
    public @ConnectionMaliciousContentStatus int getMaliciousContentStatus() {
        return ConnectionMaliciousContentStatus.NONE;
    }

    @Override
    public int getPageClassification(boolean prefetch) {
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

    void setCurrentUrl(@Nullable GURL url) {
        mGurl = url;
    }

    void setIsIncognitoForTesting(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    @Override
    public ObservableSupplier<@ControlsPosition Integer> getToolbarPositionSupplier() {
        return mToolbarPosition;
    }
}
