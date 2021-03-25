// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.res.Resources;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * A component for displaying a status icon (e.g. security icon or navigation icon) and optional
 * verbose status text.
 */
public class StatusCoordinator implements View.OnClickListener, LocationBarDataProvider.Observer {
    /** Interface for displaying page info popup on omnibox. */
    public interface PageInfoAction {
        /**
         * @param tab Tab containing the content to show page info for.
         * @param highlightedPermission The ContentSettingsType to be highlighted on the page.
         */
        void show(Tab tab, @ContentSettingsType int highlightedPermission);
    }

    // TODO(crbug.com/1109369): Do not store the StatusView
    private final StatusView mStatusView;
    private final StatusMediator mMediator;
    private final PropertyModel mModel;
    private final boolean mIsTablet;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final PageInfoAction mPageInfoAction;
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
     * @param templateUrlServiceSupplier A supplier for {@link TemplateUrlService} used to query
     *         the default search engine.
     * @param searchEngineLogoUtils Utils to query the state of the search engine logos feature.
     * @param windowAndroid The {@link WindowAndroid} that is used by the owning {@link Activity}.
     * @param pageInfoAction Displays page info popup.
     */
    public StatusCoordinator(boolean isTablet, StatusView statusView,
            UrlBarEditingTextStateProvider urlBarEditingTextStateProvider,
            IncognitoStateProvider incognitoStateProvider,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            LocationBarDataProvider locationBarDataProvider,
            Supplier<TemplateUrlService> templateUrlServiceSupplier,
            SearchEngineLogoUtils searchEngineLogoUtils, Supplier<Profile> profileSupplier,
            WindowAndroid windowAndroid, PageInfoAction pageInfoAction) {
        mIsTablet = isTablet;
        mStatusView = statusView;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mLocationBarDataProvider = locationBarDataProvider;
        mPageInfoAction = pageInfoAction;

        mModel = new PropertyModel(StatusProperties.ALL_KEYS);

        PropertyModelChangeProcessor.create(mModel, mStatusView, new StatusViewBinder());

        Runnable forceModelViewReconciliationRunnable = () -> {
            final View securityIconView = getSecurityIconView();
            mStatusView.setAlpha(1f);
            securityIconView.setAlpha(mModel.get(StatusProperties.STATUS_ICON_ALPHA));
            securityIconView.setVisibility(
                    mModel.get(StatusProperties.SHOW_STATUS_ICON) ? View.VISIBLE : View.GONE);
        };

        PageInfoIPHController pageInfoIPHController = new PageInfoIPHController(
                ContextUtils.activityFromContext(mStatusView.getContext()), getSecurityIconView());

        mMediator = new StatusMediator(mModel, mStatusView.getResources(), mStatusView.getContext(),
                urlBarEditingTextStateProvider, isTablet, forceModelViewReconciliationRunnable,
                locationBarDataProvider, PermissionDialogController.getInstance(),
                searchEngineLogoUtils, templateUrlServiceSupplier, profileSupplier,
                pageInfoIPHController, windowAndroid);

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
        mStatusView.setSearchEngineLogoUtils(searchEngineLogoUtils);
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
        mMediator.updateLocationBarIcon(StatusView.IconTransitionType.CROSSFADE);
        mMediator.setStatusClickListener(this);
    }

    /** @param urlHasFocus Whether the url currently has focus. */
    public void onUrlFocusChange(boolean urlHasFocus) {
        mMediator.setUrlHasFocus(urlHasFocus);
        mUrlHasFocus = urlHasFocus;
        updateVerboseStatusVisibility();
    }

    /** @param showExpandedState Whether the url bar is expanded currently. */
    public void onUrlAnimationFinished(boolean showExpandedState) {
        mMediator.setUrlAnimationFinished(showExpandedState);
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
    // Using the default empty onNtpStartedLoading.
    // Using the default empty onPrimaryColorChanged.
    // Using the default empty onTitleChanged.
    // Using the default empty onUrlChanged.

    @Override
    public void onIncognitoStateChanged() {
        mMediator.onIncognitoStateChanged();
    }

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

        mPageInfoAction.show(mLocationBarDataProvider.getTab(), mMediator.getLastPermission());
        mMediator.onPageInfoOpened();
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

    /**
     * Update information required to display the search engine icon.
     * @param isSearchEngineGoogle Whether the current search engine is google.
     * @param searchEngineUrl The URL for the current URL for the search engine.
     */
    public void updateSearchEngineStatusIcon(boolean isSearchEngineGoogle, String searchEngineUrl) {
        mMediator.updateSearchEngineStatusIcon(isSearchEngineGoogle, searchEngineUrl);
        // TODO(crbug.com/1109369): Do not use the StatusView here
        mStatusView.updateSearchEngineStatusIcon();
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
        mMediator.destroy();
        mLocationBarDataProvider.removeObserver(this);
        mLocationBarDataProvider = null;
    }
}
