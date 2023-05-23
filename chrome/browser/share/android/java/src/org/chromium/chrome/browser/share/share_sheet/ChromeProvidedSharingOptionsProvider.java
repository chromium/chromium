// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;
import android.content.ComponentName;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeProvidedSharingOptionsProviderBase;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareContentTypeHelper.ContentType;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsCoordinator;
import org.chromium.chrome.browser.share.screenshot.ScreenshotCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleMetricsHelper.LinkToggleMetricsDetails;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.DeviceLockActivityLauncher;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Provides {@code PropertyModel}s of Chrome-provided sharing options.
 */
public class ChromeProvidedSharingOptionsProvider extends ChromeProvidedSharingOptionsProviderBase {
    // ComponentName used for Chrome share options in ShareParams.TargetChosenCallback
    public static final ComponentName CHROME_PROVIDED_FEATURE_COMPONENT_NAME =
            new ComponentName("CHROME", "CHROME_FEATURE");

    private static final String USER_ACTION_SCREENSHOT_SELECTED =
            "SharingHubAndroid.ScreenshotSelected";
    private static final String USER_ACTION_LONG_SCREENSHOT_SELECTED =
            "SharingHubAndroid.LongScreenshotSelected";

    private final ShareSheetBottomSheetContent mBottomSheetContent;
    private final long mShareStartTime;
    private final ImageEditorModuleProvider mImageEditorModuleProvider;
    private final @LinkGeneration int mLinkGenerationStatusForMetrics;
    private final LinkToggleMetricsDetails mLinkToggleMetricsDetails;

    /**
     * Constructs a new {@link ChromeProvidedSharingOptionsProvider}.
     *
     * @param activity The current {@link Activity}.
     * @param windowAndroid The current window.
     * @param tabProvider Supplier for the current activity tab.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param bottomSheetContent The {@link ShareSheetBottomSheetContent} for the current
     * activity.
     * @param shareParams The {@link ShareParams} for the current share.
     * @param printTab A {@link Callback} that will print a given Tab.
     * @param isIncognito Whether incognito mode is enabled.
     * @param shareStartTime The start time of the current share.
     * @param chromeOptionShareCallback A ChromeOptionShareCallback that can be used by
     * Chrome-provided sharing options.
     * @param imageEditorModuleProvider Image Editor module entry point if present in the APK.
     * @param featureEngagementTracker feature engagement tracker.
     * @param url Url to share.
     * @param linkGenerationStatusForMetrics User action of sharing text from failed link-to-text
     * generation, sharing text from successful link-to-text generation, or sharing link-to-text.
     * @param linkToggleMetricsDetails {@link LinkToggleMetricsDetails} for recording the final
     *         toggle state.
     * @param profile The current profile of the User.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     */
    ChromeProvidedSharingOptionsProvider(Activity activity, WindowAndroid windowAndroid,
            Supplier<Tab> tabProvider, BottomSheetController bottomSheetController,
            ShareSheetBottomSheetContent bottomSheetContent, ShareParams shareParams,
            Callback<Tab> printTab, boolean isIncognito, long shareStartTime,
            ChromeOptionShareCallback chromeOptionShareCallback,
            ImageEditorModuleProvider imageEditorModuleProvider, Tracker featureEngagementTracker,
            String url, @LinkGeneration int linkGenerationStatusForMetrics,
            LinkToggleMetricsDetails linkToggleMetricsDetails, Profile profile,
            DeviceLockActivityLauncher deviceLockActivityLauncher) {
        super(activity, windowAndroid, tabProvider, bottomSheetController, shareParams, printTab,
                isIncognito, chromeOptionShareCallback, featureEngagementTracker, url, profile,
                deviceLockActivityLauncher);
        mBottomSheetContent = bottomSheetContent;
        mShareStartTime = shareStartTime;
        mImageEditorModuleProvider = imageEditorModuleProvider;
        mLinkGenerationStatusForMetrics = linkGenerationStatusForMetrics;
        mLinkToggleMetricsDetails = linkToggleMetricsDetails;
    }

    /**
     * Returns an ordered list of {@link PropertyModel}s that should be shown given the {@code
     * contentTypes} being shared.
     *
     * @param contentTypes a {@link Set} of {@link ContentType}.
     * @param detailedContentType the {@link DetailedContentType} being shared.
     * @param isMultiWindow if in multi-window mode.
     * @return a list of {@link PropertyModel}s.
     */
    List<PropertyModel> getPropertyModels(Set<Integer> contentTypes,
            @DetailedContentType int detailedContentType, boolean isMultiWindow) {
        List<PropertyModel> propertyModels = new ArrayList<>();
        for (FirstPartyOption firstPartyOption :
                getFirstPartyOptions(contentTypes, detailedContentType, isMultiWindow)) {
            propertyModels.add(getShareSheetModel(firstPartyOption));
        }
        return propertyModels;
    }

    private PropertyModel getShareSheetModel(FirstPartyOption option) {
        boolean setShowNewBadge = showNewBadge(option);
        boolean hideBottomSheetContentOnTap = hideBottomSheetContentOnTap(option);

        return ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, option.icon),
                mActivity.getResources().getString(option.iconLabel),
                option.iconContentDescription, (view) -> {
                    ShareSheetCoordinator.recordShareMetrics(option.featureNameForMetrics,
                            mLinkGenerationStatusForMetrics, mLinkToggleMetricsDetails,
                            mShareStartTime, mProfile);
                    if (hideBottomSheetContentOnTap) {
                        mBottomSheetController.hideContent(mBottomSheetContent, true);
                    }
                    option.onClickCallback.onResult(view);
                    callTargetChosenCallback();
                }, setShowNewBadge);
    }

    private boolean showNewBadge(FirstPartyOption firstPartyOption) {
        if (!mFeatureEngagementTracker.isInitialized()) return false;

        if (USER_ACTION_SCREENSHOT_SELECTED.equals(firstPartyOption.featureNameForMetrics)) {
            return mFeatureEngagementTracker.shouldTriggerHelpUI(
                    FeatureConstants.IPH_SHARE_SCREENSHOT_FEATURE);
        }
        if (USER_ACTION_WEB_STYLE_NOTES_SELECTED.equals(firstPartyOption.featureNameForMetrics)) {
            return mFeatureEngagementTracker.shouldTriggerHelpUI(
                    FeatureConstants.SHARING_HUB_WEBNOTES_STYLIZE_FEATURE);
        }
        return false;
    }

    private boolean hideBottomSheetContentOnTap(FirstPartyOption firstPartyOption) {
        if (USER_ACTION_SCREENSHOT_SELECTED.equals(firstPartyOption.featureNameForMetrics)
                || USER_ACTION_LONG_SCREENSHOT_SELECTED.equals(
                        firstPartyOption.featureNameForMetrics)) {
            return false;
        }
        return true;
    }

    @Override
    protected FirstPartyOption createScreenshotFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.LINK_PAGE_VISIBLE, ContentType.TEXT,
                ContentType.HIGHLIGHTED_TEXT, ContentType.IMAGE)
                .setDetailedContentTypesToDisableFor(DetailedContentType.WEB_NOTES)
                .setIcon(R.drawable.screenshot, R.string.sharing_screenshot)
                .setFeatureNameForMetrics(USER_ACTION_SCREENSHOT_SELECTED)
                .setDisableForMultiWindow(true)
                .setOnClickCallback((view) -> {
                    mFeatureEngagementTracker.notifyEvent(EventConstants.SHARE_SCREENSHOT_SELECTED);
                    ScreenshotCoordinator coordinator = new ScreenshotCoordinator(mActivity,
                            mShareParams.getWindow(), mUrl, mChromeOptionShareCallback,
                            mBottomSheetController, mImageEditorModuleProvider);
                    mBottomSheetController.addObserver(coordinator);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                })
                .build();
    }

    @Override
    protected FirstPartyOption createLongScreenshotsFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.LINK_PAGE_VISIBLE, ContentType.TEXT,
                ContentType.HIGHLIGHTED_TEXT, ContentType.IMAGE)
                .setDetailedContentTypesToDisableFor(DetailedContentType.WEB_NOTES)
                .setIcon(R.drawable.long_screenshot, R.string.sharing_long_screenshot)
                .setFeatureNameForMetrics(USER_ACTION_LONG_SCREENSHOT_SELECTED)
                .setDisableForMultiWindow(true)
                .setOnClickCallback((view) -> {
                    mFeatureEngagementTracker.notifyEvent(EventConstants.SHARE_SCREENSHOT_SELECTED);
                    LongScreenshotsCoordinator coordinator = LongScreenshotsCoordinator.create(
                            mActivity, mTabProvider.get(), mUrl, mChromeOptionShareCallback,
                            mBottomSheetController, mImageEditorModuleProvider);
                    mBottomSheetController.addObserver(coordinator);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                })
                .build();
    }

    private void callTargetChosenCallback() {
        ShareParams.TargetChosenCallback callback = mShareParams.getCallback();
        if (callback != null) {
            callback.onTargetChosen(CHROME_PROVIDED_FEATURE_COMPONENT_NAME);
            // Reset callback after onTargetChosen() is called to prevent cancel() being called when
            // the sheet is closed.
            mShareParams.setCallback(null);
        }
    }
}
