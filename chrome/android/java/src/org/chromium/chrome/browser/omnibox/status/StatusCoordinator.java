// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.app.Activity;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.page_info.ChromePageInfoControllerDelegate;
import org.chromium.chrome.browser.page_info.ChromePermissionParamsListBuilderDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * A component for displaying a status icon (e.g. security icon or navigation icon) and optional
 * verbose status text.
 */
public class StatusCoordinator implements View.OnClickListener, LocationBarDataProvider.Observer {
    // TODO(crbug.com/1109369): Do not store the StatusView
    private final StatusView mStatusView;
    private final StatusMediator mMediator;
    private final PropertyModel mModel;
    private final boolean mIsTablet;
    private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private LocationBarDataProvider mLocationBarDataProvider;
    private boolean mUrlHasFocus;

    /**
     * Creates a new {@link StatusCoordinator}.
     *
     * @param isTablet Whether the UI is shown on a tablet.
     * @param statusView The status view, used to supply and manipulate child views.
     * @param urlBarEditingTextStateProvider The url coordinator.
     * @param incognitoStateProvider Provider of incocognito-ness for the active TabModel.
     * @param modalDialogManagerSupplier A supplier for {@link ModalDialogManager} used to display a
     *         dialog.
     */
    public StatusCoordinator(boolean isTablet, StatusView statusView,
            UrlBarEditingTextStateProvider urlBarEditingTextStateProvider,
            IncognitoStateProvider incognitoStateProvider,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            LocationBarDataProvider locationBarDataProvider) {
        mIsTablet = isTablet;
        mStatusView = statusView;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mLocationBarDataProvider = locationBarDataProvider;

        mModel = new PropertyModel(StatusProperties.ALL_KEYS);

        PropertyModelChangeProcessor.create(mModel, mStatusView, new StatusViewBinder());

        Runnable forceModelViewReconciliationRunnable = () -> {
            final View securityIconView = getSecurityIconView();
            mStatusView.setAlpha(1f);
            securityIconView.setAlpha(mModel.get(StatusProperties.STATUS_ICON_ALPHA));
            securityIconView.setVisibility(
                    mModel.get(StatusProperties.SHOW_STATUS_ICON) ? View.VISIBLE : View.GONE);
        };
        mMediator = new StatusMediator(mModel, mStatusView.getResources(), mStatusView.getContext(),
                urlBarEditingTextStateProvider, isTablet, forceModelViewReconciliationRunnable,
                incognitoStateProvider, locationBarDataProvider);

        Resources res = mStatusView.getResources();
        mMediator.setUrlMinWidth(res.getDimensionPixelSize(R.dimen.location_bar_min_url_width)
                + res.getDimensionPixelSize(R.dimen.location_bar_icon_width)
                + (res.getDimensionPixelSize(R.dimen.location_bar_lateral_padding) * 2));

        mMediator.setSeparatorFieldMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_status_separator_width)
                + res.getDimensionPixelSize(R.dimen.location_bar_status_separator_spacer));

        mMediator.setVerboseStatusTextMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_min_verbose_status_text_width));

        mStatusView.setLocationBarDataProvider(mLocationBarDataProvider);
        // Update status immediately after receiving the data provider to avoid initial presence
        // glitch on tablet devices. This glitch would be typically seen upon launch of app, right
        // before the landing page is presented to the user.
        updateStatusIcon();
        updateVerboseStatusVisibility();
        mLocationBarDataProvider.addObserver(this);
    }

    /**
     * Provides data and state for the toolbar component.
     *
     * @param locationBarDataProvider The data provider.
     */
    public void setLocationBarDataProviderForTesting(
            LocationBarDataProvider locationBarDataProvider) {
        mLocationBarDataProvider.removeObserver(this);
        mLocationBarDataProvider = locationBarDataProvider;
        mMediator.setLocationBarDataProviderForTesting(mLocationBarDataProvider);
        mStatusView.setLocationBarDataProvider(mLocationBarDataProvider);
        // Update status immediately after receiving the data provider to avoid initial presence
        // glitch on tablet devices. This glitch would be typically seen upon launch of app, right
        // before the landing page is presented to the user.
        updateStatusIcon();
        updateVerboseStatusVisibility();
        mLocationBarDataProvider.addObserver(this);
    }

    /** Signals that native initialization has completed. */
    public void onNativeInitialized() {
        mMediator.updateLocationBarIcon();
        mMediator.setStatusClickListener(this);
    }

    /** @param urlHasFocus Whether the url currently has focus. */
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
     *
     * @param percent The current focus percent.
     */
    public void setUrlFocusChangePercent(float percent) {
        mMediator.setUrlFocusChangePercent(percent);
    }

    /**  @param useDarkColors Whether dark colors should be for the status icon and text. */
    public void setUseDarkColors(boolean useDarkColors) {
        mMediator.setUseDarkColors(useDarkColors);

        // TODO(ender): remove this once icon selection has complete set of
        // corresponding properties (for tinting etc).
        updateStatusIcon();
    }

    // LocationBarData.Observer implementation
    // Using the default empty onIncognitoStateChanged.
    // Using the default empty onNtpStartedLoading.
    // Using the default empty onPrimaryColorChanged.
    // Using the default empty onTitleChanged.
    // Using the default empty onUrlChanged.

    @Override
    public void onSecurityStateChanged() {
        updateStatusIcon();
        updateVerboseStatusVisibility();
    }

    /** Updates the security icon displayed in the LocationBar. */
    private void updateStatusIcon() {
        mMediator.setSecurityIconResource(
                mLocationBarDataProvider.getSecurityIconResource(mIsTablet));
        mMediator.setSecurityIconTint(mLocationBarDataProvider.getSecurityIconColorStateList());
        mMediator.setSecurityIconDescription(
                mLocationBarDataProvider.getSecurityIconContentDescriptionResourceId());
    }

    /** Returns the view displaying the security icon. */
    public View getSecurityIconView() {
        return mStatusView.getSecurityButton();
    }

    /** Returns {@code true} if the security button is currently being displayed. */
    @VisibleForTesting
    public boolean isSecurityButtonShown() {
        return mMediator.isSecurityButtonShown();
    }

    /** Returns {@code true} if the search engine status is currently being displayed. */
    public boolean isSearchEngineStatusIconVisible() {
        // TODO(crbug.com/1109369): try to hide this method
        return mStatusView.isSearchEngineStatusIconVisible();
    }

    /** Returns the ID of the drawable currently shown in the security icon. */
    @DrawableRes
    public int getSecurityIconResourceIdForTesting() {
        return mModel.get(StatusProperties.STATUS_ICON_RESOURCE) == null
                ? 0
                : mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting();
    }

    /** Returns the icon identifier used for custom resources. */
    public String getSecurityIconIdentifierForTesting() {
        return mModel.get(StatusProperties.STATUS_ICON_RESOURCE) == null
                ? null
                : mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconIdentifierForTesting();
    }

    /**
     * Update visibility of the verbose status based on the button type and focus state of the
     * omnibox.
     */
    private void updateVerboseStatusVisibility() {
        mMediator.setPageSecurityLevel(mLocationBarDataProvider.getSecurityLevel());
        mMediator.setPageIsOffline(mLocationBarDataProvider.isOfflinePage());
        mMediator.setPageIsPreview(mLocationBarDataProvider.isPreview());
        mMediator.setPageIsPaintPreview(mLocationBarDataProvider.isPaintPreview());
    }

    @Override
    public void onClick(View view) {
        if (mUrlHasFocus) return;

        // If isInOverviewAndShowingOmnibox is true, getTab isn't correct for PageInfo; if it's not
        // null, it reflects a web page that the user isn't currently looking at.
        // TODO(https://crbug.com/1150289): Add a particular page icon for start surface.
        if (!mLocationBarDataProvider.hasTab()
                || mLocationBarDataProvider.getTab().getWebContents() == null
                || mLocationBarDataProvider.isInOverviewAndShowingOmnibox()) {
            return;
        }

        Tab tab = mLocationBarDataProvider.getTab();
        WebContents webContents = tab.getWebContents();
        Activity activity = TabUtils.getActivity(tab);
        PageInfoController.show(activity, webContents, null,
                PageInfoController.OpenedFromSource.TOOLBAR,
                new ChromePageInfoControllerDelegate(activity, webContents,
                        mModalDialogManagerSupplier,
                        /*offlinePageLoadUrlDelegate=*/
                        new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab)),
                new ChromePermissionParamsListBuilderDelegate());
    }

    /**
     * Called to set the width of the location bar when the url bar is not focused.
     * This value is used to determine whether the verbose status text should be visible.
     *
     * @param width The unfocused location bar width.
     */
    public void setUnfocusedLocationBarWidth(int width) {
        mMediator.setUnfocusedLocationBarWidth(width);
    }

    /** Toggle animation of icon changes. */
    public void setShouldAnimateIconChanges(boolean shouldAnimate) {
        mMediator.setAnimationsEnabled(shouldAnimate);
    }

    /** Specify whether URL should present icons when focused. */
    public void setShowIconsWhenUrlFocused(boolean showIconsWithUrlFocused) {
        mMediator.setShowIconsWhenUrlFocused(showIconsWithUrlFocused);
    }

    /** Update information required to display the search engine icon. */
    public void updateSearchEngineStatusIcon(boolean shouldShowSearchEngineLogo,
            boolean isSearchEngineGoogle, String searchEngineUrl) {
        mMediator.updateSearchEngineStatusIcon(
                shouldShowSearchEngineLogo, isSearchEngineGoogle, searchEngineUrl);
        // TODO(crbug.com/1109369): Do not use the StatusView here
        mStatusView.updateSearchEngineStatusIcon(
                shouldShowSearchEngineLogo, isSearchEngineGoogle, searchEngineUrl);
    }

    /** Returns width of the status icon including start/end margins. */
    public int getStatusIconWidth() {
        // TODO(crbug.com/1109369): try to hide this method
        return mStatusView.getStatusIconWidth();
    }

    /** @see View#getMeasuredWidth() */
    public int getMeasuredWidth() {
        // TODO(crbug.com/1109369): try to hide this method
        return mStatusView.getMeasuredWidth();
    }

    /** Returns the increase in StatusView end padding, when the Url bar is focused. */
    public int getEndPaddingPixelSizeOnFocusDelta() {
        return mMediator.getEndPaddingPixelSizeOnFocusDelta();
    }

    /**
     * Notifies StatusCoordinator that the default match for the currently entered autocomplete text
     * has been classified, indicating whether the default match is a search.
     *
     * @param defaultMatchIsSearch Whether the default match is a search.
     */
    public void onDefaultMatchClassified(boolean defaultMatchIsSearch) {
        mMediator.updateLocationBarIconForDefaultMatchCategory(defaultMatchIsSearch);
    }

    public void destroy() {
        mLocationBarDataProvider.removeObserver(this);
        mLocationBarDataProvider = null;
    }
}
