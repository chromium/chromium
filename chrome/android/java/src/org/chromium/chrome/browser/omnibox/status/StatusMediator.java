// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.annotations.MockedInTests;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the controller logic of the Status component.
 */
class StatusMediator implements IncognitoStateProvider.IncognitoStateObserver {
    @VisibleForTesting
    @MockedInTests
    class StatusMediatorDelegate {
        /** @see {@link SearchEngineLogoUtils#getSearchEngineLogoFavicon} */
        void getSearchEngineLogoFavicon(Resources res, Callback<Bitmap> callback) {
            SearchEngineLogoUtils.getSearchEngineLogoFavicon(Profile.getLastUsedRegularProfile(),
                    res, callback, TemplateUrlServiceFactory.get());
        }

        /** @see {@link SearchEngineLogoUtils#shouldShowSearchEngineLogo} */
        boolean shouldShowSearchEngineLogo(boolean isIncognito) {
            return SearchEngineLogoUtils.shouldShowSearchEngineLogo(isIncognito);
        }

        /** @see {@link SearchEngineLogoUtils#shouldShowSearchLoupeEverywhere} */
        boolean shouldShowSearchLoupeEverywhere(boolean isIncognito) {
            return SearchEngineLogoUtils.shouldShowSearchLoupeEverywhere(isIncognito);
        }
    }

    private final PropertyModel mModel;
    private boolean mDarkTheme;
    private boolean mUrlHasFocus;
    private boolean mVerboseStatusSpaceAvailable;
    private boolean mPageIsPreview;
    private boolean mPageIsPaintPreview;
    private boolean mPageIsOffline;
    private boolean mShowStatusIconWhenUrlFocused;
    private boolean mIsSecurityButtonShown;
    private boolean mIsSearchEngineStateSetup;
    private boolean mIsSearchEngineGoogle;
    private boolean mShouldCancelCustomFavicon;
    private boolean mIsTablet;

    private final int mEndPaddingPixelSizeOnFocusDelta;
    private int mUrlMinWidth;
    private int mSeparatorMinWidth;
    private int mVerboseStatusTextMinWidth;

    private @ConnectionSecurityLevel int mPageSecurityLevel;

    private @DrawableRes int mSecurityIconRes;
    private @DrawableRes int mSecurityIconTintRes;
    private @StringRes int mSecurityIconDescriptionRes;
    private @DrawableRes int mNavigationIconTintRes;

    private StatusMediatorDelegate mDelegate;
    private Resources mResources;
    private Context mContext;

    private LocationBarDataProvider mLocationBarDataProvider;
    private UrlBarEditingTextStateProvider mUrlBarEditingTextStateProvider;

    private boolean mUrlBarTextIsSearch = true;

    private float mUrlFocusPercent;
    private String mSearchEngineLogoUrl;

    private boolean mIsIncognito;
    private Runnable mForceModelViewReconciliationRunnable;

    // Factors used to offset the animation of the status icon's alpha adjustment. The full formula
    // used: alpha = (focusAnimationProgress - mTextOffsetThreshold) / (1 - mTextOffsetThreshold)
    // mTextOffsetThreshold will be the % space that the icon takes up during the focus animation.
    // When un/focused padding(s) change, this formula shouldn't need to change.
    private final float mTextOffsetThreshold;
    // The denominator for the above formula, which will adjust the scale for the alpha.
    private final float mTextOffsetAdjustedScale;

    StatusMediator(PropertyModel model, Resources resources, Context context,
            UrlBarEditingTextStateProvider urlBarEditingTextStateProvider, boolean isTablet,
            Runnable forceModelViewReconciliationRunnable,
            IncognitoStateProvider incognitoStateProvider,
            LocationBarDataProvider locationBarDataProvider) {
        mModel = model;
        mLocationBarDataProvider = locationBarDataProvider;
        mDelegate = new StatusMediatorDelegate();
        updateColorTheme();

        mResources = resources;
        mContext = context;
        mUrlBarEditingTextStateProvider = urlBarEditingTextStateProvider;

        mEndPaddingPixelSizeOnFocusDelta =
                mResources.getDimensionPixelSize(R.dimen.sei_location_bar_icon_end_padding_focused)
                - mResources.getDimensionPixelSize(R.dimen.sei_location_bar_icon_end_padding);
        int iconWidth = resources.getDimensionPixelSize(R.dimen.location_bar_status_icon_width);
        mTextOffsetThreshold =
                (float) iconWidth / (iconWidth + getEndPaddingPixelSizeOnFocusDelta());
        mTextOffsetAdjustedScale = mTextOffsetThreshold == 1 ? 1 : (1 - mTextOffsetThreshold);

        mIsTablet = isTablet;
        mForceModelViewReconciliationRunnable = forceModelViewReconciliationRunnable;
        if (incognitoStateProvider != null) {
            incognitoStateProvider.addIncognitoStateObserverAndTrigger(this);
        }
    }

    /**
     * Override the LocationBarDataProvider for this class for testing purposes.
     */
    void setLocationBarDataProviderForTesting(LocationBarDataProvider locationBarDataProvider) {
        mLocationBarDataProvider = locationBarDataProvider;
    }

    /**
     * Toggle animations of icon changes.
     */
    void setAnimationsEnabled(boolean enabled) {
        mModel.set(StatusProperties.ANIMATIONS_ENABLED, enabled);
    }

    /**
     * Specify whether displayed page is an offline page.
     */
    void setPageIsOffline(boolean pageIsOffline) {
        if (mPageIsOffline != pageIsOffline) {
            mPageIsOffline = pageIsOffline;
            updateStatusVisibility();
            updateColorTheme();
        }
    }

    /**
     * Specify whether displayed page is a preview page.
     */
    void setPageIsPreview(boolean pageIsPreview) {
        if (mPageIsPreview != pageIsPreview) {
            mPageIsPreview = pageIsPreview;
            updateStatusVisibility();
            updateColorTheme();
        }
    }

    /**
     * Specify whether displayed page is a preview page.
     */
    void setPageIsPaintPreview(boolean pageIsPaintPreview) {
        if (mPageIsPaintPreview != pageIsPaintPreview) {
            mPageIsPaintPreview = pageIsPaintPreview;
            updateStatusVisibility();
            updateColorTheme();
        }
    }

    /**
     * Specify displayed page's security level.
     */
    void setPageSecurityLevel(@ConnectionSecurityLevel int level) {
        if (mPageSecurityLevel == level) return;
        mPageSecurityLevel = level;
        updateStatusVisibility();
        updateLocationBarIcon();
    }

    /**
     * Specify icon displayed by the security chip.
     */
    void setSecurityIconResource(@DrawableRes int securityIcon) {
        mSecurityIconRes = securityIcon;
        updateLocationBarIcon();
    }

    /**
     * Specify tint of icon displayed by the security chip.
     */
    void setSecurityIconTint(@ColorRes int tintList) {
        mSecurityIconTintRes = tintList;
        updateLocationBarIcon();
    }

    /**
     * Specify tint of icon displayed by the security chip.
     */
    void setSecurityIconDescription(@StringRes int desc) {
        mSecurityIconDescriptionRes = desc;
        updateLocationBarIcon();
    }

    /**
     * Specify minimum width of the separator field.
     */
    void setSeparatorFieldMinWidth(int width) {
        mSeparatorMinWidth = width;
    }

    /**
     * Returns the increase in StatusView end padding, when the Url bar is focused.
     */
    int getEndPaddingPixelSizeOnFocusDelta() {
        return mEndPaddingPixelSizeOnFocusDelta;
    }

    /**
     * Specify whether status icon should be shown when URL is focused.
     */
    void setShowIconsWhenUrlFocused(boolean showIconWhenFocused) {
        mShowStatusIconWhenUrlFocused = showIconWhenFocused;
        updateLocationBarIcon();
    }

    /**
     * Specify object to receive status click events.
     *
     * @param listener Specifies target object to receive events.
     */
    void setStatusClickListener(View.OnClickListener listener) {
        mModel.set(StatusProperties.STATUS_CLICK_LISTENER, listener);
    }

    /**
     * Update unfocused location bar width to determine shape and content of the
     * Status view.
     */
    void setUnfocusedLocationBarWidth(int width) {
        // This unfocused width is used rather than observing #onMeasure() to avoid showing the
        // verbose status when the animation to unfocus the URL bar has finished. There is a call to
        // LocationBarLayout#onMeasure() after the URL focus animation has finished and before the
        // location bar has received its updated width layout param.
        int computedSpace = width - mUrlMinWidth - mSeparatorMinWidth;
        boolean hasSpaceForStatus = width >= mVerboseStatusTextMinWidth;

        if (hasSpaceForStatus) {
            mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_WIDTH, computedSpace);
        }

        if (hasSpaceForStatus != mVerboseStatusSpaceAvailable) {
            mVerboseStatusSpaceAvailable = hasSpaceForStatus;
            updateStatusVisibility();
        }
    }

    /**
     * Report URL focus change.
     */
    void setUrlHasFocus(boolean urlHasFocus) {
        if (mUrlHasFocus == urlHasFocus) return;

        mUrlHasFocus = urlHasFocus;
        updateStatusVisibility();
        updateLocationBarIcon();

        // Set the default match to be a search on an unfocus event to avoid the globe sticking
        // around for subsequent focus events.
        if (!mUrlHasFocus) updateLocationBarIconForDefaultMatchCategory(true);
    }

    // Extra logic to support extra NTP use cases which show the status icon when animating and when
    // focused, but hide it when unfocused.
    void setUrlAnimationFinished(boolean urlHasFocus) {
        // On tablets, the status icon should always be shown so the following logic doesn't apply.
        assert !mIsTablet : "This logic shouldn't be called on tablets";

        if (!mDelegate.shouldShowSearchEngineLogo(mIsIncognito)) {
            return;
        }

        // Hide the icon when the url unfocus animation finishes.
        // Note: When mUrlFocusPercent is non-zero, that means we're still in the focused state from
        // scrolling on the NTP.
        if (!urlHasFocus && MathUtils.areFloatsEqual(mUrlFocusPercent, 0f)
                && SearchEngineLogoUtils.currentlyOnNTP(mLocationBarDataProvider)) {
            setStatusIconShown(false);
        }
    }

    void setStatusIconShown(boolean show) {
        mModel.set(StatusProperties.SHOW_STATUS_ICON, show);
    }

    /**
     * Set the url focus change percent.
     * @param percent The current focus percent.
     */
    void setUrlFocusChangePercent(float percent) {
        mUrlFocusPercent = percent;
        // On tablets, the status icon should always be shown so the following logic doesn't apply.
        assert !mIsTablet : "This logic shouldn't be called on tablets";

        if (!mDelegate.shouldShowSearchEngineLogo(mIsIncognito)) {
            return;
        }

        // Note: This uses mUrlFocusPercent rather than mUrlHasFocus because when the user scrolls
        // the NTP we want the status icon to show.
        if (mUrlFocusPercent > 0) {
            setStatusIconShown(true);
        }

        // Only fade the animation on the new tab page.
        if (SearchEngineLogoUtils.currentlyOnNTP(mLocationBarDataProvider)) {
            float focusAnimationProgress = percent;
            if (!mUrlHasFocus) {
                focusAnimationProgress = MathUtils.clamp(
                        (percent - mTextOffsetThreshold) / mTextOffsetAdjustedScale, 0f, 1f);
            }
            mModel.set(StatusProperties.STATUS_ICON_ALPHA, focusAnimationProgress);
        } else {
            mModel.set(StatusProperties.STATUS_ICON_ALPHA, 1f);
        }

        updateLocationBarIcon();
    }

    /**
     * Specify minimum width of an URL field.
     */
    void setUrlMinWidth(int width) {
        mUrlMinWidth = width;
    }

    /**
     * Toggle between dark and light UI color theme.
     */
    void setUseDarkColors(boolean useDarkColors) {
        if (mDarkTheme != useDarkColors) {
            mDarkTheme = useDarkColors;
            updateColorTheme();
        }
    }

    /**
     * Specify minimum width of the verbose status text field.
     */
    void setVerboseStatusTextMinWidth(int width) {
        mVerboseStatusTextMinWidth = width;
    }

    /**
     * Update visibility of the verbose status text field.
     */
    private void updateStatusVisibility() {
        int statusText = 0;

        if (mPageIsPaintPreview) {
            statusText = R.string.location_bar_paint_preview_page_status;
        } else if (mPageIsPreview) {
            statusText = R.string.location_bar_preview_lite_page_status;
        } else if (mPageIsOffline) {
            statusText = R.string.location_bar_verbose_status_offline;
        }

        // Decide whether presenting verbose status text makes sense.
        boolean newVisibility = shouldShowVerboseStatusText() && mVerboseStatusSpaceAvailable
                && (!mUrlHasFocus) && (statusText != 0);

        // Update status content only if it is visible.
        // Note: PropertyModel will help us avoid duplicate updates with the
        // same value.
        if (newVisibility) {
            mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES, statusText);
        }

        mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE, newVisibility);
    }

    /**
     * Update color theme for all status components.
     */
    private void updateColorTheme() {
        @ColorRes
        int separatorColor =
                mDarkTheme ? R.color.divider_line_bg_color_dark : R.color.divider_line_bg_color_light;

        @ColorRes
        int textColor = 0;
        if (mPageIsPreview || mPageIsPaintPreview) {
            textColor = mDarkTheme ? R.color.locationbar_status_preview_color
                                   : R.color.locationbar_status_preview_color_light;
        } else if (mPageIsOffline) {
            textColor = mDarkTheme ? R.color.locationbar_status_offline_color
                                   : R.color.locationbar_status_offline_color_light;
        }

        @ColorRes
        int tintColor = ThemeUtils.getThemedToolbarIconTintRes(!mDarkTheme);

        mModel.set(StatusProperties.SEPARATOR_COLOR_RES, separatorColor);
        mNavigationIconTintRes = tintColor;
        if (textColor != 0) mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES, textColor);

        updateLocationBarIcon();
    }

    /**
     * Reports whether security icon is shown.
     */
    @VisibleForTesting
    boolean isSecurityButtonShown() {
        return mIsSecurityButtonShown;
    }

    /**
     * Compute verbose status text for the current page.
     */
    private boolean shouldShowVerboseStatusText() {
        return (mPageIsPreview && mPageSecurityLevel != ConnectionSecurityLevel.DANGEROUS)
                || mPageIsOffline || mPageIsPaintPreview;
    }

    /**
     * Called when the search engine status icon needs updating.
     *
     * @param shouldShowSearchEngineLogo True if the search engine icon should be shown.
     * @param isSearchEngineGoogle True if the default search engine is google.
     * @param searchEngineUrl The URL for the search engine icon.
     */
    public void updateSearchEngineStatusIcon(boolean shouldShowSearchEngineLogo,
            boolean isSearchEngineGoogle, String searchEngineUrl) {
        mIsSearchEngineStateSetup = true;
        mIsSearchEngineGoogle = isSearchEngineGoogle;
        mSearchEngineLogoUrl = searchEngineUrl;
        updateLocationBarIcon();
    }

    /**
     * Update selection of icon presented on the location bar.
     *
     * - Navigation button is:
     *     - shown only on large form factor devices (tablets and up),
     *     - shown only if URL is focused.
     *
     * - Security icon is:
     *     - shown only if specified,
     *     - not shown if URL is focused.
     */
    void updateLocationBarIcon() {
        // Update the accessibility description before continuing since we need it either way.
        mModel.set(StatusProperties.STATUS_ICON_DESCRIPTION_RES, getAccessibilityDescriptionRes());

        // No need to proceed further if we've already updated it for the search engine icon.
        if (!LibraryLoader.getInstance().isInitialized()
                || maybeUpdateStatusIconForSearchEngineIcon()) {
            return;
        }

        int icon = 0;
        int tint = 0;
        int toast = 0;

        mIsSecurityButtonShown = false;
        if (mUrlHasFocus) {
            if (mShowStatusIconWhenUrlFocused) {
                icon = mUrlBarTextIsSearch ? R.drawable.ic_suggestion_magnifier
                                           : R.drawable.ic_globe_24dp;
                tint = mNavigationIconTintRes;
            }
        } else if (mSecurityIconRes != 0) {
            mIsSecurityButtonShown = true;
            icon = mSecurityIconRes;
            tint = mSecurityIconTintRes;
            toast = R.string.menu_page_info;
        }

        if (mPageIsPreview) {
            tint = mDarkTheme ? R.color.locationbar_status_preview_color
                              : R.color.locationbar_status_preview_color_light;
        }

        mModel.set(StatusProperties.STATUS_ICON_RESOURCE,
                icon == 0 ? null : new StatusIconResource(icon, tint));
        mModel.set(StatusProperties.STATUS_ICON_ACCESSIBILITY_TOAST_RES, toast);
    }

    /** @return True if the security icon has been set for the search engine icon. */
    @VisibleForTesting
    boolean maybeUpdateStatusIconForSearchEngineIcon() {
        boolean showIconWhenFocused = mUrlHasFocus && mShowStatusIconWhenUrlFocused;
        boolean showIconWhenScrollingOnNTP =
                SearchEngineLogoUtils.currentlyOnNTP(mLocationBarDataProvider)
                && mUrlFocusPercent > 0 && !mUrlHasFocus && !mLocationBarDataProvider.isLoading()
                && mShowStatusIconWhenUrlFocused;
        // Show the logo unfocused if we're on the NTP.
        if (mDelegate.shouldShowSearchEngineLogo(mIsIncognito) && mIsSearchEngineStateSetup
                && (showIconWhenFocused || showIconWhenScrollingOnNTP)) {
            getStatusIconResourceForSearchEngineIcon(mIsIncognito, (statusIconRes) -> {
                mModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIconRes);
            });
            return true;
        } else {
            mShouldCancelCustomFavicon = true;
            return false;
        }
    }

    /**
     * Set the security icon resource for the search engine icon and invoke the callback to inform
     * the caller which resource has been set.
     *
     * @param isIncognito True if the user is incognito.
     * @param resourceCallback Called when the final value is set for the security icon resource.
     *                         Meant to give the caller a chance to set the tint for the given
     *                         resource.
     */
    private void getStatusIconResourceForSearchEngineIcon(
            boolean isIncognito, Callback<StatusIconResource> resourceCallback) {
        mShouldCancelCustomFavicon = false;
        // If the current url text is a valid url, then swap the dse icon for a globe.
        if (!mUrlBarTextIsSearch) {
            resourceCallback.onResult(new StatusIconResource(R.drawable.ic_globe_24dp,
                    getSecurityIconTintForSearchEngineIcon(R.drawable.ic_globe_24dp)));
        } else if (mIsSearchEngineGoogle) {
            if (mDelegate.shouldShowSearchLoupeEverywhere(isIncognito)) {
                resourceCallback.onResult(new StatusIconResource(R.drawable.ic_search,
                        getSecurityIconTintForSearchEngineIcon(R.drawable.ic_search)));
            } else {
                resourceCallback.onResult(
                        new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0));
            }
        } else {
            if (mDelegate.shouldShowSearchLoupeEverywhere(isIncognito)) {
                resourceCallback.onResult(new StatusIconResource(R.drawable.ic_search,
                        getSecurityIconTintForSearchEngineIcon(R.drawable.ic_search)));
            } else {
                getNonGoogleSearchEngineIconBitmap(
                        statusIconResource -> { resourceCallback.onResult(statusIconResource); });
            }
        }
    }

    /** @return The non-Google search engine icon {@link Bitmap}. */
    private void getNonGoogleSearchEngineIconBitmap(final Callback<StatusIconResource> callback) {
        mDelegate.getSearchEngineLogoFavicon(mResources, (favicon) -> {
            if (favicon == null || mShouldCancelCustomFavicon) {
                callback.onResult(new StatusIconResource(R.drawable.ic_search,
                        getSecurityIconTintForSearchEngineIcon(R.drawable.ic_search)));
                return;
            }

            callback.onResult(new StatusIconResource(mSearchEngineLogoUrl, favicon, 0));
        });
    }

    /**
     * Get the icon tint for the given search engine icon resource.
     * @param icon The icon resource for the search engine icon.
     * @return The tint resource for the given parameters.
     */
    @VisibleForTesting
    int getSecurityIconTintForSearchEngineIcon(int icon) {
        int tint;
        if (icon == 0 || icon == R.drawable.ic_logo_googleg_20dp) {
            tint = 0;
        } else {
            tint = mDarkTheme ? R.color.default_icon_color_secondary_tint_list
                              : ThemeUtils.getThemedToolbarIconTintRes(!mDarkTheme);
        }

        return tint;
    }

    /** Return the resource id for the accessibility description or 0 if none apply. */
    private int getAccessibilityDescriptionRes() {
        if (mUrlHasFocus) {
            if (SearchEngineLogoUtils.shouldShowSearchEngineLogo(mIsIncognito)) {
                return 0;
            } else if (mShowStatusIconWhenUrlFocused) {
                return R.string.accessibility_toolbar_btn_site_info;
            }
        } else if (mSecurityIconRes != 0) {
            return mSecurityIconDescriptionRes;
        }

        return 0;
    }

    /**
     *  Informs StatusMediator that the default match may have changed categories, updating the
     * status icon if it has.
     */
    /* package */ void updateLocationBarIconForDefaultMatchCategory(boolean defaultMatchIsSearch) {
        if (defaultMatchIsSearch != mUrlBarTextIsSearch) {
            mUrlBarTextIsSearch = defaultMatchIsSearch;
            updateLocationBarIcon();
        }
    }

    @VisibleForTesting
    protected String resolveUrlBarTextWithAutocomplete(CharSequence urlBarText) {
        String currentAutocompleteText = mUrlBarEditingTextStateProvider.getTextWithAutocomplete();
        String urlTextWithAutocomplete;
        if (TextUtils.isEmpty(urlBarText)) {
            // TODO (crbug.com/1012870): This is to workaround the UrlBar text being empty but the
            // autocomplete text still pointing at the previous url's autocomplete text.
            urlTextWithAutocomplete = "";
        } else if (TextUtils.indexOf(currentAutocompleteText, urlBarText) > -1) {
            // TODO(crbug.com/1015147): This is to workaround the UrlBar text pointing to the
            // "current" url and the the autocomplete text pointing to the "previous" url.
            urlTextWithAutocomplete = currentAutocompleteText;
        } else {
            // If the above cases don't apply, then we should use the UrlBar text itself.
            urlTextWithAutocomplete = urlBarText.toString();
        }

        return urlTextWithAutocomplete;
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        boolean previousIsIncognito = mIsIncognito;
        mIsIncognito = isIncognito;
        boolean incognitoBadgeVisible = isIncognito && !mIsTablet;
        mModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, incognitoBadgeVisible);
        if (previousIsIncognito != isIncognito) reconcileVisualState();
    }

    /**
     * Temporary workaround for the divergent logic for status icon visibility changes for the dse
     * icon experiment. Should be removed when the dse icon launches (crbug.com/1019488).
     *
     * When transitioning to incognito, the first visible view when focused will be assigned to
     * UrlBar. When the UrlBar is the first visible view when focused, the StatusView's alpha
     * will be set to 0 in LocationBarPhone#populateFadeAnimations. When transitioning back from
     * incognito, StatusView's state needs to be reset to match the current state of the status view
     * {@link org.chromium.chrome.browser.omnibox.LocationBarPhone#updateVisualsForState}.
     * property model.
     **/
    private void reconcileVisualState() {
        // No reconciliation is needed on tablet because the status icon is always shown.
        if (mIsTablet) return;

        if (!mShowStatusIconWhenUrlFocused || mIsIncognito
                || !mDelegate.shouldShowSearchEngineLogo(mIsIncognito)) {
            return;
        }

        assert mForceModelViewReconciliationRunnable != null;
        mForceModelViewReconciliationRunnable.run();
    }

    void setDelegateForTesting(StatusMediatorDelegate delegate) {
        mDelegate = delegate;
    }
}
