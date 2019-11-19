// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.res.Resources;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.page_info.PageInfoController;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * A component for displaying a status icon (e.g. security icon or navigation icon) and optional
 * verbose status text.
 */
public class StatusViewCoordinator implements View.OnClickListener, TextWatcher {
    private final StatusView mStatusView;
    private final StatusMediator mMediator;
    private final PropertyModel mModel;
    private final boolean mIsTablet;
    private ToolbarDataProvider mToolbarDataProvider;
    private boolean mUrlHasFocus;

    /**
     * Creates a new StatusViewCoordinator.
     * @param isTablet Whether the UI is shown on a tablet.
     * @param statusView The status view, used to supply and manipulate child views.
     * @param urlBarEditingTextStateProvider The url coordinator.
     */
    public StatusViewCoordinator(boolean isTablet, StatusView statusView,
            UrlBarEditingTextStateProvider urlBarEditingTextStateProvider) {
        mIsTablet = isTablet;
        mStatusView = statusView;

        mModel = new PropertyModel.Builder(StatusProperties.ALL_KEYS)
                         .with(StatusProperties.STATUS_ICON_TINT_RES, R.color.divider_bg_color)
                         .build();

        PropertyModelChangeProcessor.create(mModel, mStatusView, new StatusViewBinder());
        mMediator = new StatusMediator(
                mModel, mStatusView.getResources(), urlBarEditingTextStateProvider);

        Resources res = mStatusView.getResources();
        mMediator.setUrlMinWidth(res.getDimensionPixelSize(R.dimen.location_bar_min_url_width)
                + res.getDimensionPixelSize(R.dimen.location_bar_start_icon_width)
                + (res.getDimensionPixelSize(R.dimen.location_bar_lateral_padding) * 2));

        mMediator.setSeparatorFieldMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_status_separator_width)
                + res.getDimensionPixelSize(R.dimen.location_bar_status_separator_spacer));

        mMediator.setVerboseStatusTextMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_min_verbose_status_text_width));
    }

    /**
     * Provides data and state for the toolbar component.
     * @param toolbarDataProvider The data provider.
     */
    public void setToolbarDataProvider(ToolbarDataProvider toolbarDataProvider) {
        mToolbarDataProvider = toolbarDataProvider;
        mMediator.setToolbarCommonPropertiesModel(mToolbarDataProvider);
        mStatusView.setToolbarCommonPropertiesModel(mToolbarDataProvider);
        // Update status immediately after receiving the data provider to avoid initial presence
        // glitch on tablet devices. This glitch would be typically seen upon launch of app, right
        // before the landing page is presented to the user.
        updateStatusIcon();
    }

    /**
     * Signals that native initialization has completed.
     */
    public void onNativeInitialized() {
        mMediator.setStatusClickListener(this);
    }

    /**
     * @param urlHasFocus Whether the url currently has focus.
     */
    public void onUrlFocusChange(boolean urlHasFocus) {
        mMediator.setUrlHasFocus(urlHasFocus);
        mUrlHasFocus = urlHasFocus;
        updateVerboseStatusVisibility();
    }

    /** @param urlHasFocus Whether the url currently has focus. */
    public void onUrlAnimationFinished(boolean urlHasFocus) {
        mMediator.setUrlAnimationFinished(urlHasFocus);
    }

    /** @param show Whether the status icon should be VISIBLE, otherwise GONE. */
    public void setStatusIconShown(boolean show) {
        mMediator.setStatusIconShown(show);
    }

    /**
     * Set the url focus change percent.
     * @param percent The current focus percent.
     */
    public void setUrlFocusChangePercent(float percent) {
        mMediator.setUrlFocusChangePercent(percent);
    }

    /**
     * @param useDarkColors Whether dark colors should be for the status icon and text.
     */
    public void setUseDarkColors(boolean useDarkColors) {
        mMediator.setUseDarkColors(useDarkColors);

        // TODO(ender): remove this once icon selection has complete set of
        // corresponding properties (for tinting etc).
        updateStatusIcon();
    }

    /**
     * @param incognitoBadgeVisible Whether or not the incognito badge is visible.
     */
    public void setIncognitoBadgeVisibility(boolean incognitoBadgeVisible) {
        mMediator.setIncognitoBadgeVisibility(incognitoBadgeVisible);
    }

    /**
     * Updates the security icon displayed in the LocationBar.
     */
    public void updateStatusIcon() {
        mMediator.setSecurityIconResource(mToolbarDataProvider.getSecurityIconResource(mIsTablet));
        mMediator.setSecurityIconTint(mToolbarDataProvider.getSecurityIconColorStateList());
        mMediator.setSecurityIconDescription(
                mToolbarDataProvider.getSecurityIconContentDescription());

        // TODO(ender): drop these during final cleanup round.
        updateVerboseStatusVisibility();
    }

    /**
     * @return The view displaying the security icon.
     */
    public View getSecurityIconView() {
        return mStatusView.getSecurityButton();
    }

    /**
     * @return Whether the security button is currently being displayed.
     */
    @VisibleForTesting
    public boolean isSecurityButtonShown() {
        return mMediator.isSecurityButtonShown();
    }

    /**
     * @return The ID of the drawable currently shown in the security icon.
     */
    @VisibleForTesting
    @DrawableRes
    public int getSecurityIconResourceId() {
        return mModel.get(StatusProperties.STATUS_ICON_RES);
    }

    /**
     * Update visibility of the verbose status based on the button type and focus state of the
     * omnibox.
     */
    private void updateVerboseStatusVisibility() {
        // TODO(ender): turn around logic for ToolbarDataProvider to offer
        // notifications rather than polling for these attributes.
        mMediator.setPageSecurityLevel(mToolbarDataProvider.getSecurityLevel());
        mMediator.setPageIsOffline(mToolbarDataProvider.isOfflinePage());
        mMediator.setPageIsPreview(mToolbarDataProvider.isPreview());
    }

    @Override
    public void onClick(View view) {
        if (mUrlHasFocus) return;

        if (!mToolbarDataProvider.hasTab()
                || mToolbarDataProvider.getTab().getWebContents() == null) {
            return;
        }

        PageInfoController.show(mToolbarDataProvider.getTab().getActivity(),
                mToolbarDataProvider.getTab(), null, PageInfoController.OpenedFromSource.TOOLBAR);
    }

    /**
     * Called to set the width of the location bar when the url bar is not focused.
     * This value is used to determine whether the verbose status text should be visible.
     * @param width The unfocused location bar width.
     */
    public void setUnfocusedLocationBarWidth(int width) {
        mMediator.setUnfocusedLocationBarWidth(width);
    }

    /**
     * Toggle animation of icon changes.
     */
    public void setShouldAnimateIconChanges(boolean shouldAnimate) {
        mMediator.setAnimationsEnabled(shouldAnimate);
    }

    /**
     * Specify whether URL should present icons when focused.
     */
    public void setShowIconsWhenUrlFocused(boolean showIconsWithUrlFocused) {
        mMediator.setShowIconsWhenUrlFocused(showIconsWithUrlFocused);
    }

    /**
     * Specify whether suggestion for URL bar is a search action.
     */
    public void setFirstSuggestionIsSearchType(boolean firstSuggestionIsSearchQuery) {
        mMediator.setFirstSuggestionIsSearchType(firstSuggestionIsSearchQuery);
    }

    /**
     * Update information required to display the search engine icon.
     */
    public void updateSearchEngineStatusIcon(boolean shouldShowSearchEngineLogo,
            boolean isSearchEngineGoogle, String searchEngineUrl) {
        mMediator.updateSearchEngineStatusIcon(
                shouldShowSearchEngineLogo, isSearchEngineGoogle, searchEngineUrl);
    }

    /**
     * @return Width of the status icon including start/end margins.
     */
    public int getStatusIconWidth() {
        return mStatusView.getStatusIconWidth();
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void onTextChanged(CharSequence charSequence, int start, int before, int count) {
        mMediator.onTextChanged(charSequence);
    }

    @Override
    public void afterTextChanged(Editable editable) {}
}
