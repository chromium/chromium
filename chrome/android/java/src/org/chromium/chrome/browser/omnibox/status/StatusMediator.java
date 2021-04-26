// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.PermissionIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusView.IconTransitionType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoDiscoverabilityMetrics;
import org.chromium.components.page_info.PageInfoDiscoverabilityMetrics.DiscoverabilityAction;
import org.chromium.components.page_info.PageInfoFeatureList;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the controller logic of the Status component.
 */
public class StatusMediator implements PermissionDialogController.Observer {
    private static final int PERMISSION_ICON_DISPLAY_TIMEOUT_MS = 8500;

    private final PropertyModel mModel;
    private final SearchEngineLogoUtils mSearchEngineLogoUtils;
    private final Supplier<TemplateUrlService> mTemplateUrlServiceSupplier;
    private final Supplier<Profile> mProfileSupplier;
    private boolean mDarkTheme;
    private boolean mUrlHasFocus;
    private boolean mVerboseStatusSpaceAvailable;
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

    private Resources mResources;
    private Context mContext;

    private LocationBarDataProvider mLocationBarDataProvider;
    private UrlBarEditingTextStateProvider mUrlBarEditingTextStateProvider;

    private final PermissionDialogController mPermissionDialogController;
    private final Handler mPermissionTaskHandler = new Handler();
    @ContentSettingsType
    private int mLastPermission = ContentSettingsType.DEFAULT;
    private final PageInfoIPHController mPageInfoIPHController;
    private final PageInfoDiscoverabilityMetrics mDiscoverabilityMetrics =
            new PageInfoDiscoverabilityMetrics();
    private final WindowAndroid mWindowAndroid;

    private boolean mUrlBarTextIsSearch = true;

    private float mUrlFocusPercent;
    private String mSearchEngineLogoUrl;

    private Runnable mForceModelViewReconciliationRunnable;

    // Factors used to offset the animation of the status icon's alpha adjustment. The full formula
    // used: alpha = (focusAnimationProgress - mTextOffsetThreshold) / (1 - mTextOffsetThreshold)
    // mTextOffsetThreshold will be the % space that the icon takes up during the focus animation.
    // When un/focused padding(s) change, this formula shouldn't need to change.
    private final float mTextOffsetThreshold;
    // The denominator for the above formula, which will adjust the scale for the alpha.
    private final float mTextOffsetAdjustedScale;

    /**
     * @param model The {@link PropertyModel} for this mediator.
     * @param resources Used to load resources.
     * @param context The {@link Context} for this Status component.
     * @param urlBarEditingTextStateProvider Provides url bar text state.
     * @param isTablet Whether the current device is a tablet.
     * @param locationBarDataProvider Provides data to the location bar.
     * @param permissionDialogController Controls showing permission dialogs.
     * @param searchEngineLogoUtils Provides utilities around the search engine logo.
     * @param templateUrlServiceSupplier Supplies the {@link TemplateUrlService}.
     * @param profileSupplier Supplies the current {@link Profile}.
     * @param pageInfoIPHController Manages when an IPH bubble for PageInfo is shown.
     * @param windowAndroid The current {@link WindowAndroid}.
     */
    public StatusMediator(PropertyModel model, Resources resources, Context context,
            UrlBarEditingTextStateProvider urlBarEditingTextStateProvider, boolean isTablet,
            Runnable forceModelViewReconciliationRunnable,
            LocationBarDataProvider locationBarDataProvider,
            PermissionDialogController permissionDialogController,
            SearchEngineLogoUtils searchEngineLogoUtils,
            Supplier<TemplateUrlService> templateUrlServiceSupplier,
            Supplier<Profile> profileSupplier, PageInfoIPHController pageInfoIPHController,
            WindowAndroid windowAndroid) {
        mModel = model;
        mLocationBarDataProvider = locationBarDataProvider;
        mSearchEngineLogoUtils = searchEngineLogoUtils;
        mTemplateUrlServiceSupplier = templateUrlServiceSupplier;
        mProfileSupplier = profileSupplier;
        updateColorTheme();

        mResources = resources;
        mContext = context;
        mUrlBarEditingTextStateProvider = urlBarEditingTextStateProvider;
        mPageInfoIPHController = pageInfoIPHController;
        mWindowAndroid = windowAndroid;

        mEndPaddingPixelSizeOnFocusDelta =
                mResources.getDimensionPixelSize(R.dimen.sei_location_bar_icon_end_padding_focused)
                - mResources.getDimensionPixelSize(R.dimen.sei_location_bar_icon_end_padding);
        int iconWidth = resources.getDimensionPixelSize(R.dimen.location_bar_status_icon_width);
        mTextOffsetThreshold =
                (float) iconWidth / (iconWidth + getEndPaddingPixelSizeOnFocusDelta());
        mTextOffsetAdjustedScale = mTextOffsetThreshold == 1 ? 1 : (1 - mTextOffsetThreshold);

        mIsTablet = isTablet;
        mForceModelViewReconciliationRunnable = forceModelViewReconciliationRunnable;
        mPermissionDialogController = permissionDialogController;
        mPermissionDialogController.addObserver(this);
    }

    public void destroy() {
        mPermissionTaskHandler.removeCallbacksAndMessages(null);
        mPermissionDialogController.removeObserver(this);
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
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    /**
     * Specify icon displayed by the security chip.
     */
    void setSecurityIconResource(@DrawableRes int securityIcon) {
        mSecurityIconRes = securityIcon;
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    /**
     * Specify tint of icon displayed by the security chip.
     */
    void setSecurityIconTint(@ColorRes int tintList) {
        mSecurityIconTintRes = tintList;
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    /**
     * Specify tint of icon displayed by the security chip.
     */
    void setSecurityIconDescription(@StringRes int desc) {
        mSecurityIconDescriptionRes = desc;
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
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
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
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
        updateLocationBarIcon(IconTransitionType.CROSSFADE);

        // Set the default match to be a search on an unfocus event to avoid the globe sticking
        // around for subsequent focus events.
        if (!mUrlHasFocus) updateLocationBarIconForDefaultMatchCategory(true);
    }

    /**
     * Extra logic to support extra NTP use cases which show the status icon when animating and when
     * focused, but hide it when unfocused.
     * @param showExpandedState Whether the url bar is expanded currently.
     */
    void setUrlAnimationFinished(boolean showExpandedState) {
        if (mIsTablet
                || !mSearchEngineLogoUtils.shouldShowSearchEngineLogo(
                        mLocationBarDataProvider.isIncognito())) {
            return;
        }

        // Hide the icon when the url unfocus animation finishes.
        // Note: When mUrlFocusPercent is non-zero, that means we're still in the focused state from
        // scrolling on the NTP.
        if (!showExpandedState && MathUtils.areFloatsEqual(mUrlFocusPercent, 0f)
                && mSearchEngineLogoUtils.currentlyOnNTP(mLocationBarDataProvider)) {
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

        if (!mSearchEngineLogoUtils.shouldShowSearchEngineLogo(
                    mLocationBarDataProvider.isIncognito())) {
            return;
        }

        // Note: This uses mUrlFocusPercent rather than mUrlHasFocus because when the user scrolls
        // the NTP we want the status icon to show.
        if (mUrlFocusPercent > 0) {
            setStatusIconShown(true);
        }

        // Only fade the animation on the new tab page.
        if (mSearchEngineLogoUtils.currentlyOnNTP(mLocationBarDataProvider)) {
            float focusAnimationProgress = percent;
            if (!mUrlHasFocus) {
                focusAnimationProgress = MathUtils.clamp(
                        (percent - mTextOffsetThreshold) / mTextOffsetAdjustedScale, 0f, 1f);
            }
            mModel.set(StatusProperties.STATUS_ICON_ALPHA, focusAnimationProgress);
        } else {
            mModel.set(StatusProperties.STATUS_ICON_ALPHA, 1f);
        }

        updateLocationBarIcon(IconTransitionType.CROSSFADE);
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
        if (mPageIsPaintPreview) {
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

        updateLocationBarIcon(IconTransitionType.CROSSFADE);
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
        return mPageIsOffline || mPageIsPaintPreview;
    }

    /**
     * Called when the search engine status icon needs updating.
     *
     * @param isSearchEngineGoogle True if the default search engine is google.
     * @param searchEngineUrl The URL for the search engine icon.
     */
    public void updateSearchEngineStatusIcon(boolean isSearchEngineGoogle, String searchEngineUrl) {
        mIsSearchEngineStateSetup = true;
        mIsSearchEngineGoogle = isSearchEngineGoogle;
        mSearchEngineLogoUrl = searchEngineUrl;
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
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
    void updateLocationBarIcon(@IconTransitionType int transitionType) {
        // Reset the last saved permission.
        mLastPermission = ContentSettingsType.DEFAULT;
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

        StatusIconResource statusIcon = icon == 0 ? null : new StatusIconResource(icon, tint);
        if (statusIcon != null) {
            statusIcon.setTransitionType(transitionType);
        }
        mModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIcon);
        mModel.set(StatusProperties.STATUS_ICON_ACCESSIBILITY_TOAST_RES, toast);
    }

    /** @return True if the security icon has been set for the search engine icon. */
    @VisibleForTesting
    boolean maybeUpdateStatusIconForSearchEngineIcon() {
        // Show the logo unfocused if we're on the NTP.
        if (shouldUpdateStatusIconForSearchEngineIcon()) {
            getStatusIconResourceForSearchEngineIcon(
                    mLocationBarDataProvider.isIncognito(), (statusIconRes) -> {
                        // Check again in case the conditions have changed since this callback was
                        // created.
                        if (shouldUpdateStatusIconForSearchEngineIcon()) {
                            mModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIconRes);
                        }
                    });
            return true;
        } else {
            mShouldCancelCustomFavicon = true;
            return false;
        }
    }

    private boolean shouldUpdateStatusIconForSearchEngineIcon() {
        boolean showIconWhenFocused = mUrlHasFocus && mShowStatusIconWhenUrlFocused;
        boolean showIconWhenScrollingOnNTP =
                mSearchEngineLogoUtils.currentlyOnNTP(mLocationBarDataProvider)
                && mUrlFocusPercent > 0 && !mUrlHasFocus && !mLocationBarDataProvider.isLoading()
                && mShowStatusIconWhenUrlFocused;

        return mSearchEngineLogoUtils.shouldShowSearchEngineLogo(
                       mLocationBarDataProvider.isIncognito())
                && mIsSearchEngineStateSetup && (showIconWhenFocused || showIconWhenScrollingOnNTP);
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
                    ThemeUtils.getThemedToolbarIconTintRes(/* useLight= */ !mDarkTheme)));
        } else {
            mSearchEngineLogoUtils.getSearchEngineLogo(mResources, mDarkTheme,
                    mProfileSupplier.get(), mTemplateUrlServiceSupplier.get(), resourceCallback);
        }
    }

    /** Return the resource id for the accessibility description or 0 if none apply. */
    private int getAccessibilityDescriptionRes() {
        if (mUrlHasFocus) {
            if (mSearchEngineLogoUtils.shouldShowSearchEngineLogo(
                        mLocationBarDataProvider.isIncognito())) {
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
            updateLocationBarIcon(IconTransitionType.CROSSFADE);
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

    public void onIncognitoStateChanged() {
        boolean incognitoBadgeVisible = mLocationBarDataProvider.isIncognito() && !mIsTablet;
        mModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, incognitoBadgeVisible);
        reconcileVisualState();
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

        if (!mShowStatusIconWhenUrlFocused || mLocationBarDataProvider.isIncognito()
                || !mSearchEngineLogoUtils.shouldShowSearchEngineLogo(
                        mLocationBarDataProvider.isIncognito())) {
            return;
        }

        assert mForceModelViewReconciliationRunnable != null;
        mForceModelViewReconciliationRunnable.run();
    }

    // PermissionDialogController.Observer interface
    @Override
    public void onDialogResult(WindowAndroid window, @ContentSettingsType int[] permissions,
            @ContentSettingValues int result) {
        if (!PageInfoFeatureList.isEnabled(PageInfoFeatureList.PAGE_INFO_DISCOVERABILITY)
                || window != mWindowAndroid) {
            return;
        }
        @ContentSettingsType
        int permission = SingleWebsiteSettings.getHighestPriorityPermission(permissions);
        // The permission is not available in the settings page. Do not show an icon.
        if (permission == ContentSettingsType.DEFAULT) return;
        mLastPermission = permission;

        boolean isIncognito = mLocationBarDataProvider.isIncognito();
        Drawable permissionDrawable = ContentSettingsResources.getIconForOmnibox(
                mContext, mLastPermission, result, isIncognito);
        PermissionIconResource permissionIconResource =
                new PermissionIconResource(permissionDrawable, isIncognito);
        permissionIconResource.setTransitionType(IconTransitionType.ROTATE);
        // We only want to notify the IPH controller after the icon transition is finished.
        // IPH is controlled by the FeatureEngagement system through finch with a field trial
        // testing configuration.
        permissionIconResource.setAnimationFinishedCallback(this::startIPH);
        // Set the timer to switch the icon back afterwards.
        mPermissionTaskHandler.removeCallbacksAndMessages(null);
        mModel.set(StatusProperties.STATUS_ICON_RESOURCE, permissionIconResource);
        mPermissionTaskHandler.postDelayed(
                ()
                        -> updateLocationBarIcon(IconTransitionType.ROTATE),
                PERMISSION_ICON_DISPLAY_TIMEOUT_MS);
        mDiscoverabilityMetrics.recordDiscoverabilityAction(
                DiscoverabilityAction.PERMISSION_ICON_SHOWN);
    }

    private void startIPH() {
        mPageInfoIPHController.onPermissionDialogShown(getIPHTimeout());
    }

    /**
     * @return A timeout for the IPH bubble. The bubble is shown after the permission icon animation
     * finishes and should disappear when it animates out.
     */
    private static int getIPHTimeout() {
        return PERMISSION_ICON_DISPLAY_TIMEOUT_MS - (2 * StatusView.ICON_ROTATION_DURATION_MS);
    }

    /** Notifies that the page info was opened. */
    void onPageInfoOpened() {
        if (mLastPermission != ContentSettingsType.DEFAULT) {
            mDiscoverabilityMetrics.recordDiscoverabilityAction(
                    DiscoverabilityAction.PAGE_INFO_OPENED);
            mPermissionTaskHandler.removeCallbacksAndMessages(null);
            updateLocationBarIcon(IconTransitionType.CROSSFADE);
        }
    }

    public int getLastPermission() {
        return mLastPermission;
    }
}
