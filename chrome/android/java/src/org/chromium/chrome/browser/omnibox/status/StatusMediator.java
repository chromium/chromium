// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinatorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.ToolbarColors;
import org.chromium.chrome.browser.toolbar.ToolbarCommonPropertiesModel;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the controller logic of the Status component.
 */
class StatusMediator {
    @VisibleForTesting
    class StatusMediatorDelegate {
        /** @see {@link AutocompleteCoordinatorFactory#qualifyPartialURLQuery} */
        boolean isUrlValid(String partialUrl) {
            return BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                           .isFullBrowserStarted()
                    && AutocompleteCoordinatorFactory.qualifyPartialURLQuery(partialUrl) != null;
        }

        /** @see {@link SearchEngineLogoUtils#getSearchEngineLogoFavicon} */
        void getSearchEngineLogoFavicon(Resources res, Callback<Bitmap> callback) {
            SearchEngineLogoUtils.getSearchEngineLogoFavicon(
                    Profile.getLastUsedProfile().getOriginalProfile(), res, callback);
        }

        /** @see {@link SearchEngineLogoUtils#shouldShowSearchEngineLogo} */
        boolean shouldShowSearchEngineLogo(boolean isIncognito) {
            return SearchEngineLogoUtils.shouldShowSearchEngineLogo(isIncognito);
        }

        /** @see {@link SearchEngineLogoUtils#shouldShowSearchLoupeEverywhere} */
        boolean shouldShowSearchLoupeEverywhere(boolean isIncognito) {
            return SearchEngineLogoUtils.shouldShowSearchLoupeEverywhere(isIncognito);
        }

        /** @see {@link SearchEngineLogoUtils#doesUrlMatchDefaultSearchEngine} */
        boolean doesUrlMatchDefaultSearchEngine(String url) {
            return SearchEngineLogoUtils.doesUrlMatchDefaultSearchEngine(url);
        }
    }

    private final PropertyModel mModel;
    private boolean mDarkTheme;
    private boolean mUrlHasFocus;
    private boolean mFirstSuggestionIsSearchQuery;
    private boolean mVerboseStatusSpaceAvailable;
    private boolean mPageIsPreview;
    private boolean mPageIsOffline;
    private boolean mShowStatusIconWhenUrlFocused;
    private boolean mIsSecurityButtonShown;
    private boolean mIsSearchEngineStateSetup;
    private boolean mIsSearchEngineGoogle;
    private boolean mShouldCancelCustomFavicon;

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
    private ToolbarCommonPropertiesModel mToolbarCommonPropertiesModel;
    private UrlBarEditingTextStateProvider mUrlBarEditingTextStateProvider;
    private String mUrlBarTextWithAutocomplete = "";
    private boolean mUrlBarTextIsValidUrl;
    private float mUrlFocusPercent;
    // Factors used to offset the animation of the status icon's alpha adjustment. The full formula
    // used: alpha = (focusAnimationProgress - mTextOffsetThreshold) / (1 - mTextOffsetThreshold)
    // mTextOffsetThreshold will be the % space that the icon takes up during the focus animation.
    // When un/focused padding(s) change, this formula shouldn't need to change.
    private final float mTextOffsetThreshold;
    // The denominator for the above formula, which will adjust the scale for the alpha.
    private final float mTextOffsetAdjustedScale;

    StatusMediator(PropertyModel model, Resources resources,
            UrlBarEditingTextStateProvider urlBarEditingTextStateProvider) {
        mModel = model;
        mDelegate = new StatusMediatorDelegate();
        updateColorTheme();

        mResources = resources;
        mUrlBarEditingTextStateProvider = urlBarEditingTextStateProvider;

        int iconWidth = resources.getDimensionPixelSize(R.dimen.location_bar_status_icon_width);
        mTextOffsetThreshold = (float) iconWidth
                / (iconWidth
                        + resources.getDimensionPixelSize(
                                R.dimen.sei_location_bar_icon_end_padding_focused)
                        - resources.getDimensionPixelSize(
                                R.dimen.sei_location_bar_icon_end_padding));
        mTextOffsetAdjustedScale = mTextOffsetThreshold == 1 ? 1 : (1 - mTextOffsetThreshold);
    }

    /**
     * Set the ToolbarDataProvider for this class.
     */
    void setToolbarCommonPropertiesModel(
            ToolbarCommonPropertiesModel toolbarCommonPropertiesModel) {
        mToolbarCommonPropertiesModel = toolbarCommonPropertiesModel;
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
    }

    void setUrlAnimationFinished(boolean urlHasFocus) {
        if (!mDelegate.shouldShowSearchEngineLogo(mToolbarCommonPropertiesModel.isIncognito())) {
            return;
        }

        // Hide the icon when the url unfocus animation finishes.
        // Note: When mUrlFocusPercent is non-zero, that means we're still in the focused state from
        // scrolling on the NTP.
        if (!urlHasFocus && MathUtils.areFloatsEqual(mUrlFocusPercent, 0f)
                && SearchEngineLogoUtils.currentlyOnNTP(mToolbarCommonPropertiesModel)) {
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
        if (!mDelegate.shouldShowSearchEngineLogo(mToolbarCommonPropertiesModel.isIncognito())) {
            return;
        }

        // Note: This uses mUrlFocusPercent rather than mUrlHasFocus because when the user scrolls
        // the NTP we want the status icon to show.
        if (mUrlFocusPercent > 0) {
            setStatusIconShown(true);
        }

        // Only fade the animation on the new tab page.
        if (SearchEngineLogoUtils.currentlyOnNTP(mToolbarCommonPropertiesModel)) {
            float focusAnimationProgress = percent;
            if (!mUrlHasFocus) {
                focusAnimationProgress = MathUtils.clamp(
                        (percent - mTextOffsetThreshold) / mTextOffsetAdjustedScale, 0f, 1f);
            }
            mModel.set(StatusProperties.STATUS_ALPHA, focusAnimationProgress);
        } else {
            mModel.set(StatusProperties.STATUS_ALPHA, 1f);
        }

        updateLocationBarIcon();
    }

    /**
     * Reports whether the first omnibox suggestion is a search query.
     */
    void setFirstSuggestionIsSearchType(boolean firstSuggestionIsSearchQuery) {
        mFirstSuggestionIsSearchQuery = firstSuggestionIsSearchQuery;
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
     * @param incognitoBadgeVisible Whether or not the incognito badge is visible.
     */
    void setIncognitoBadgeVisibility(boolean incognitoBadgeVisible) {
        mModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, incognitoBadgeVisible);
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

        if (mPageIsPreview) {
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
                mDarkTheme ? R.color.divider_bg_color_dark : R.color.divider_bg_color_light;

        @ColorRes
        int textColor = 0;
        if (mPageIsPreview) {
            textColor = mDarkTheme ? R.color.locationbar_status_preview_color
                                   : R.color.locationbar_status_preview_color_light;
        } else if (mPageIsOffline) {
            textColor = mDarkTheme ? R.color.locationbar_status_offline_color
                                   : R.color.locationbar_status_offline_color_light;
        }

        @ColorRes
        int tintColor = ToolbarColors.getThemedToolbarIconTintRes(!mDarkTheme);

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
                || mPageIsOffline;
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
    private void updateLocationBarIcon() {
        // Update the accessibility description before continuing since we need it either way.
        mModel.set(StatusProperties.STATUS_ICON_DESCRIPTION_RES, getAccessibilityDescriptionRes());

        // No need to proceed further if we've already updated it for the search engine icon.
        if (maybeUpdateStatusIconForSearchEngineIcon()) return;

        int icon = 0;
        int tint = 0;
        int toast = 0;

        mIsSecurityButtonShown = false;
        if (mUrlHasFocus) {
            if (mShowStatusIconWhenUrlFocused) {
                icon = mFirstSuggestionIsSearchQuery ? R.drawable.omnibox_search
                                                     : R.drawable.ic_omnibox_page;
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

        mModel.set(StatusProperties.STATUS_ICON_RES, icon);
        mModel.set(StatusProperties.STATUS_ICON_TINT_RES, tint);
        mModel.set(StatusProperties.STATUS_ICON_ACCESSIBILITY_TOAST_RES, toast);
    }

    /** @return True if the security icon has been set for the search engine icon. */
    @VisibleForTesting
    boolean maybeUpdateStatusIconForSearchEngineIcon() {
        // When the search engine logo should be shown, but the engine isn't Google. In this case,
        // we download the icon on the fly.
        boolean showFocused =
                (mUrlHasFocus || mUrlFocusPercent > 0) && mShowStatusIconWhenUrlFocused;
        // Show the logo unfocused if "Query in the omnibox" is active or we're on the NTP. Current
        // "Query in the omnibox" behavior makes it active for non-dse searches if you've just
        // changed your default search engine.The included workaround below
        // (doesUrlMatchDefaultSearchEngine) can be removed once this is fixed.
        // TODO(crbug.com/991017): Remove doesUrlMatchDefaultSearchEngine when "Query in the
        //                         omnibox" properly reacts to dse changes.
        boolean showUnfocusedSearchResultsPage = !mUrlHasFocus
                && mToolbarCommonPropertiesModel != null
                && mToolbarCommonPropertiesModel.getDisplaySearchTerms() != null
                && mDelegate.doesUrlMatchDefaultSearchEngine(
                        mToolbarCommonPropertiesModel.getCurrentUrl());
        boolean isIncognito = mToolbarCommonPropertiesModel != null
                && mToolbarCommonPropertiesModel.isIncognito();
        if (mDelegate.shouldShowSearchEngineLogo(isIncognito) && mIsSearchEngineStateSetup
                && (showFocused || showUnfocusedSearchResultsPage)) {
            setSecurityIconResourceForSearchEngineIcon(isIncognito, (icon) -> {
                mModel.set(StatusProperties.STATUS_ICON_TINT_RES,
                        getSecurityIconTintForSearchEngineIcon(icon));
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
     * @param callback Called when the final value is set for the security icon resource. Meant to
     *                 give the caller a chance to set the tint for the given resource.
     */
    private void setSecurityIconResourceForSearchEngineIcon(
            boolean isIncognito, Callback<Integer> callback) {
        mShouldCancelCustomFavicon = false;
        // If the current url text is a valid url, then swap the dse icon for a globe.
        if (mUrlBarTextIsValidUrl) {
            mModel.set(StatusProperties.STATUS_ICON_RES, R.drawable.ic_globe_24dp);
            callback.onResult(R.drawable.ic_globe_24dp);
        } else if (mIsSearchEngineGoogle) {
            int icon = mDelegate.shouldShowSearchLoupeEverywhere(isIncognito)
                    ? R.drawable.ic_search
                    : R.drawable.ic_logo_googleg_24dp;
            mModel.set(StatusProperties.STATUS_ICON_RES, icon);
            callback.onResult(icon);
        } else {
            mModel.set(StatusProperties.STATUS_ICON_RES, R.drawable.ic_search);
            callback.onResult(R.drawable.ic_search);
            if (!mDelegate.shouldShowSearchLoupeEverywhere(isIncognito)) {
                mDelegate.getSearchEngineLogoFavicon(mResources, (favicon) -> {
                    if (favicon == null || mShouldCancelCustomFavicon) return;
                    mModel.set(StatusProperties.STATUS_ICON, favicon);
                    callback.onResult(0);
                });
            }
        }
    }

    /**
     * Get the icon tint for the given search engine icon resource.
     * @param icon The icon resource for the search engine icon.
     * @return The tint resource for the given parameters.
     */
    @VisibleForTesting
    int getSecurityIconTintForSearchEngineIcon(int icon) {
        int tint;
        if (icon == 0 || icon == R.drawable.ic_logo_googleg_24dp) {
            tint = 0;
        } else {
            tint = mDarkTheme ? R.color.default_icon_color_secondary_list
                              : ToolbarColors.getThemedToolbarIconTintRes(!mDarkTheme);
        }

        return tint;
    }

    /** Return the resource id for the accessibility description or 0 if none apply. */
    private int getAccessibilityDescriptionRes() {
        if (mUrlHasFocus) {
            if (SearchEngineLogoUtils.shouldShowSearchEngineLogo(
                        mToolbarCommonPropertiesModel.isIncognito())) {
                return 0;
            } else if (mShowStatusIconWhenUrlFocused) {
                return R.string.accessibility_toolbar_btn_site_info;
            }
        } else if (mSecurityIconRes != 0) {
            return mSecurityIconDescriptionRes;
        }

        return 0;
    }

    /** @see android.text.TextWatcher#onTextChanged */
    void onTextChanged(CharSequence urlBarText) {
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

        if (TextUtils.equals(mUrlBarTextWithAutocomplete, urlTextWithAutocomplete)) return;

        mUrlBarTextWithAutocomplete = urlTextWithAutocomplete;
        boolean isValid = mDelegate.isUrlValid(mUrlBarTextWithAutocomplete);
        if (isValid != mUrlBarTextIsValidUrl) {
            mUrlBarTextIsValidUrl = isValid;
            updateLocationBarIcon();
        }
    }

    void setDelegateForTesting(StatusMediatorDelegate delegate) {
        mDelegate = delegate;
    }
}
