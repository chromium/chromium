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
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.security_state.ConnectionSecurityLevel;

class SearchBoxDataProvider implements LocationBarDataProvider {
    private final @ColorInt int mPrimaryColor;
    private final @ColorInt int mDropdownStandardBgColor;
    private final @ColorInt int mDropdownIncognitoBgColor;
    private final @ColorInt int mSuggestionStandardBgColor;
    private final @ColorInt int mSuggestionIncognitoBgColor;
    private boolean mIsFromQuickActionSearchWidget;
    private Tab mTab;

    /**
     * @param context The {@link Context} for accessing colors.
     * @param isFromQuickActionSearchWidget
     */
    SearchBoxDataProvider(Context context) {
        mIsFromQuickActionSearchWidget = false;
        mPrimaryColor = ChromeColors.getPrimaryBackgroundColor(context, isIncognito());
        mDropdownStandardBgColor = ChromeColors.getSurfaceColor(
                context, R.dimen.omnibox_suggestion_dropdown_bg_elevation);
        mDropdownIncognitoBgColor = context.getColor(R.color.omnibox_dropdown_bg_incognito);
        mSuggestionStandardBgColor =
                ChromeColors.getSurfaceColor(context, R.dimen.omnibox_suggestion_bg_elevation);
        mSuggestionIncognitoBgColor = context.getColor(R.color.omnibox_suggestion_bg_incognito);
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
    public String getCurrentUrl() {
        return SearchActivityPreferencesManager.getCurrent().searchEngineUrl;
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
    public int getPageClassification(boolean isFocusedFromFakebox) {
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

    @Override
    public int getDropdownStandardBackgroundColor() {
        return mDropdownStandardBgColor;
    }

    @Override
    public int getDropdownIncognitoBackgroundColor() {
        return mDropdownIncognitoBgColor;
    }

    @Override
    public int getSuggestionStandardBackgroundColor() {
        return mSuggestionStandardBgColor;
    }

    @Override
    public int getSuggestionIncognitoBackgroundColor() {
        return mSuggestionIncognitoBgColor;
    }

    void setIsFromQuickActionSearchWidget(boolean isFromQuickActionsWidget) {
        mIsFromQuickActionSearchWidget = isFromQuickActionsWidget;
    }
}
