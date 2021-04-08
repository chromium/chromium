// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsCoordinator;
import org.chromium.chrome.browser.share.qrcode.QrCodeCoordinator;
import org.chromium.chrome.browser.share.screenshot.ScreenshotCoordinator;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * Provides {@code PropertyModel}s of Chrome-provided sharing options.
 */
class ChromeProvidedSharingOptionsProvider {
    private final Activity mActivity;
    private final Supplier<Tab> mTabProvider;
    private final BottomSheetController mBottomSheetController;
    private final ShareSheetBottomSheetContent mBottomSheetContent;
    private final ShareParams mShareParams;
    private final Callback<Tab> mPrintTabCallback;
    private final SettingsLauncher mSettingsLauncher;
    private final boolean mIsSyncEnabled;
    private final long mShareStartTime;
    private final List<FirstPartyOption> mOrderedFirstPartyOptions;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private ScreenshotCoordinator mScreenshotCoordinator;
    private final String mUrl;
    private final ImageEditorModuleProvider mImageEditorModuleProvider;
    private final Tracker mFeatureEngagementTracker;
    private String mShareDetailsForMetrics;

    /**
     * Constructs a new {@link ChromeProvidedSharingOptionsProvider}.
     *
     * @param activity The current {@link Activity}.
     * @param tabProvider Supplier for the current activity tab.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param bottomSheetContent The {@link ShareSheetBottomSheetContent} for the current
     * activity.
     * @param shareParams The {@link ShareParams} for the current share.
     * @param chromeShareExtras The {@link ChromeShareExtras} for the current share.
     * @param printTab A {@link Callback} that will print a given Tab.
     * @param shareStartTime The start time of the current share.
     * @param chromeOptionShareCallback A ChromeOptionShareCallback that can be used by
     * Chrome-provided sharing options.
     * @param imageEditorModuleProvider Image Editor module entry point if present in the APK.
     * @param featureEngagementTracker feature engagement tracker.
     * @param url Url to share.
     * @param shareDetailsForMetrics User action of sharing text from failed link-to-text
     *         generation,
     * sharing text from successful link-to-text generation, or sharing link-to-text.
     */
    ChromeProvidedSharingOptionsProvider(Activity activity, Supplier<Tab> tabProvider,
            BottomSheetController bottomSheetController,
            ShareSheetBottomSheetContent bottomSheetContent, ShareParams shareParams,
            ChromeShareExtras chromeShareExtras, Callback<Tab> printTab,
            SettingsLauncher settingsLauncher, boolean isSyncEnabled, long shareStartTime,
            ChromeOptionShareCallback chromeOptionShareCallback,
            ImageEditorModuleProvider imageEditorModuleProvider, Tracker featureEngagementTracker,
            String url, @Nullable String shareDetailsForMetrics) {
        mActivity = activity;
        mTabProvider = tabProvider;
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mShareParams = shareParams;
        mPrintTabCallback = printTab;
        mSettingsLauncher = settingsLauncher;
        mIsSyncEnabled = isSyncEnabled;
        mShareStartTime = shareStartTime;
        mImageEditorModuleProvider = imageEditorModuleProvider;
        mFeatureEngagementTracker = featureEngagementTracker;
        mOrderedFirstPartyOptions = new ArrayList<>();
        initializeFirstPartyOptionsInOrder();
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mUrl = url;
        mShareDetailsForMetrics = shareDetailsForMetrics;
    }

    /**
     * Encapsulates a {@link PropertyModel} and the {@link ContentType}s it should be shown for.
     */
    private static class FirstPartyOption {
        final Collection<Integer> mContentTypes;
        final Collection<Integer> mContentTypesToDisableFor;
        final PropertyModel mPropertyModel;
        final boolean mDisableForMultiWindow;

        /**
         * Should only be used when the default property model constructed in the builder does not
         * fit the feature's needs. This should be rare.
         *
         * @param model Property model for the first party option.
         * @param contentTypes Content types to trigger for.
         * @param contentTypesToDisableFor Content types to disable for.
         * @param disableForMultiWindow If the feature should be disabled if in multi-window mode.
         */
        FirstPartyOption(PropertyModel model, Collection<Integer> contentTypes,
                Collection<Integer> contentTypesToDisableFor, boolean disableForMultiWindow) {
            mPropertyModel = model;
            mContentTypes = contentTypes;
            mContentTypesToDisableFor = contentTypesToDisableFor;
            mDisableForMultiWindow = disableForMultiWindow;
        }
    }

    private class FirstPartyOptionBuilder {
        private int mIcon;
        private int mIconLabel;
        private String mFeatureNameForMetrics;
        private Callback<View> mOnClickCallback;
        private boolean mDisableForMultiWindow;
        private Integer[] mContentTypesToDisableFor;
        private final Integer[] mContentTypesInBuilder;

        FirstPartyOptionBuilder(Integer... contentTypes) {
            mContentTypesInBuilder = contentTypes;
            mContentTypesToDisableFor = new Integer[] {};
        }

        FirstPartyOptionBuilder setIcon(int icon, int iconLabel) {
            mIcon = icon;
            mIconLabel = iconLabel;
            return this;
        }

        FirstPartyOptionBuilder setFeatureNameForMetrics(String featureName) {
            mFeatureNameForMetrics = featureName;
            return this;
        }

        FirstPartyOptionBuilder setOnClickCallback(Callback<View> onClickCallback) {
            mOnClickCallback = onClickCallback;
            return this;
        }

        FirstPartyOptionBuilder setContentTypesToDisableFor(Integer... contentTypesToDisableFor) {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)) {
                mContentTypesToDisableFor = contentTypesToDisableFor;
            }
            return this;
        }

        FirstPartyOptionBuilder setDisableForMultiWindow(boolean disableForMultiWindow) {
            mDisableForMultiWindow = disableForMultiWindow;
            return this;
        }

        FirstPartyOption build() {
            PropertyModel model = ShareSheetPropertyModelBuilder.createPropertyModel(
                    AppCompatResources.getDrawable(mActivity, mIcon),
                    mActivity.getResources().getString(mIconLabel), (view) -> {
                        RecordUserAction.record(mFeatureNameForMetrics);
                        if (ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)
                                && mShareDetailsForMetrics != null) {
                            RecordUserAction.record(mShareDetailsForMetrics);
                        }
                        recordTimeToShare(mShareStartTime);
                        mBottomSheetController.hideContent(mBottomSheetContent, true);
                        mOnClickCallback.onResult(view);
                    }, /*showNewBadge*/ false);
            return new FirstPartyOption(model, Arrays.asList(mContentTypesInBuilder),
                    Arrays.asList(mContentTypesToDisableFor), mDisableForMultiWindow);
        }
    }

    /**
     * Returns an ordered list of {@link PropertyModel}s that should be shown given the {@code
     * contentTypes} being shared.
     *
     * @param contentTypes a {@link Set} of {@link ContentType}.
     * @param isMultiWindow if in multi-window mode.
     * @return a list of {@link PropertyModel}s.
     */
    List<PropertyModel> getPropertyModels(Set<Integer> contentTypes, boolean isMultiWindow) {
        List<PropertyModel> propertyModels = new ArrayList<>();
        for (FirstPartyOption firstPartyOption : mOrderedFirstPartyOptions) {
            if (!Collections.disjoint(contentTypes, firstPartyOption.mContentTypes)
                    && Collections.disjoint(
                            contentTypes, firstPartyOption.mContentTypesToDisableFor)
                    && !(isMultiWindow && firstPartyOption.mDisableForMultiWindow)) {
                propertyModels.add(firstPartyOption.mPropertyModel);
            }
        }
        return propertyModels;
    }

    /**
     * Creates all enabled {@link FirstPartyOption}s and adds them to {@code
     * mOrderedFirstPartyOptions} in the order they should appear.
     */
    private void initializeFirstPartyOptionsInOrder() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_SCREENSHOT)) {
            mOrderedFirstPartyOptions.add(createScreenshotFirstPartyOption());
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT) &&
            ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_SCREENSHOT)) {
            mOrderedFirstPartyOptions.add(createLongScreenshotsFirstPartyOption());
        }
        mOrderedFirstPartyOptions.add(createCopyLinkFirstPartyOption());
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)) {
            mOrderedFirstPartyOptions.add(createCopyImageFirstPartyOption());
            mOrderedFirstPartyOptions.add(createCopyFirstPartyOption());
            mOrderedFirstPartyOptions.add(createCopyTextFirstPartyOption());
        }
        mOrderedFirstPartyOptions.add(createSendTabToSelfFirstPartyOption());
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_HIGHLIGHTS_ANDROID)
                && !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)) {
            mOrderedFirstPartyOptions.add(createHighlightsFirstPartyOption());
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_QRCODE)
                && !mTabProvider.get().getWebContents().isIncognito()) {
            mOrderedFirstPartyOptions.add(createQrCodeFirstPartyOption());
        }
        if (UserPrefs.get(Profile.getLastUsedRegularProfile()).getBoolean(Pref.PRINTING_ENABLED)) {
            mOrderedFirstPartyOptions.add(createPrintingFirstPartyOption());
        }
    }

    /**
     * Used to initiate the screenshot flow once the bottom sheet is fully hidden. Removes itself
     * from {@link BottomSheetController} afterwards.
     */
    private final BottomSheetObserver mSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetStateChanged(int newState) {
            if (newState == SheetState.HIDDEN) {
                assert mScreenshotCoordinator != null;
                mScreenshotCoordinator.captureScreenshot();
                // Clean up the observer since the coordinator is discarded when sheet is hidden.
                mBottomSheetController.removeObserver(mSheetObserver);
            }
        }
    };

    private FirstPartyOption createScreenshotFirstPartyOption() {
        Drawable icon = AppCompatResources.getDrawable(mActivity, R.drawable.screenshot);
        boolean showNewBadge = mFeatureEngagementTracker.isInitialized()
                && mFeatureEngagementTracker.shouldTriggerHelpUI(
                        FeatureConstants.IPH_SHARE_SCREENSHOT_FEATURE);

        PropertyModel propertyModel = ShareSheetPropertyModelBuilder.createPropertyModel(
                icon, mActivity.getResources().getString(R.string.sharing_screenshot), (view) -> {
                    RecordUserAction.record("SharingHubAndroid.ScreenshotSelected");
                    recordTimeToShare(mShareStartTime);
                    mFeatureEngagementTracker.notifyEvent(EventConstants.SHARE_SCREENSHOT_SELECTED);
                    mScreenshotCoordinator = new ScreenshotCoordinator(mActivity,
                            mTabProvider.get(), mChromeOptionShareCallback, mBottomSheetController,
                            mImageEditorModuleProvider);
                    // Capture a screenshot once the bottom sheet is fully hidden. The
                    // observer will then remove itself.
                    mBottomSheetController.addObserver(mSheetObserver);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                }, showNewBadge);

        return new FirstPartyOption(propertyModel,
                Arrays.asList(ContentType.LINK_PAGE_VISIBLE, ContentType.TEXT,
                        ContentType.HIGHLIGHTED_TEXT, ContentType.IMAGE),
                /*contentTypesToDisableFor=*/Collections.emptySet(),
                /*disableForMultiWindow=*/true);
    }

    private FirstPartyOption createLongScreenshotsFirstPartyOption() {
        PropertyModel propertyModel = ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.long_screenshot),
                mActivity.getResources().getString(R.string.sharing_long_screenshot), (view) -> {
                    RecordUserAction.record("SharingHubAndroid.LongScreenshotSelected");
                    recordTimeToShare(mShareStartTime);
                    mScreenshotCoordinator = LongScreenshotsCoordinator.create(mActivity,
                            mTabProvider.get(), mChromeOptionShareCallback, mBottomSheetController,
                            mImageEditorModuleProvider);
                    // Capture a screenshot once the bottom sheet is fully hidden. The
                    // observer will then remove itself.
                    mBottomSheetController.addObserver(mSheetObserver);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                }, /*showNewBadge*/ false);
        return new FirstPartyOption(propertyModel,
                Arrays.asList(ContentType.LINK_PAGE_VISIBLE, ContentType.TEXT,
                        ContentType.HIGHLIGHTED_TEXT, ContentType.IMAGE),
                /*contentTypesToDisableFor=*/Collections.emptySet(),
                /*disableForMultiWindow=*/true);
    }

    private FirstPartyOption createCopyLinkFirstPartyOption() {
        return new FirstPartyOptionBuilder(
                ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE)
                .setContentTypesToDisableFor(ContentType.LINK_AND_TEXT)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_url)
                .setFeatureNameForMetrics("SharingHubAndroid.CopyURLSelected")
                .setOnClickCallback((view) -> {
                    ClipboardManager clipboard = (ClipboardManager) mActivity.getSystemService(
                            Context.CLIPBOARD_SERVICE);
                    clipboard.setPrimaryClip(
                            ClipData.newPlainText(mShareParams.getTitle(), mShareParams.getUrl()));
                    Toast.makeText(mActivity, R.string.link_copied, Toast.LENGTH_SHORT).show();
                })
                .build();
    }

    private FirstPartyOption createCopyImageFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.IMAGE)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_image)
                .setFeatureNameForMetrics("SharingHubAndroid.CopyImageSelected")
                .setOnClickCallback((view) -> {
                    if (!mShareParams.getFileUris().isEmpty()) {
                        Clipboard.getInstance().setImageUri(mShareParams.getFileUris().get(0));
                        Toast.makeText(mActivity, R.string.image_copied, Toast.LENGTH_SHORT).show();
                    }
                })
                .build();
    }

    private FirstPartyOption createCopyFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.LINK_AND_TEXT)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy)
                .setFeatureNameForMetrics("SharingHubAndroid.CopySelected")
                .setOnClickCallback((view) -> {
                    ClipboardManager clipboard = (ClipboardManager) mActivity.getSystemService(
                            Context.CLIPBOARD_SERVICE);
                    clipboard.setPrimaryClip(ClipData.newPlainText(
                            mShareParams.getTitle(), mShareParams.getTextAndUrl()));
                    Toast.makeText(mActivity, R.string.sharing_copied, Toast.LENGTH_SHORT).show();
                })
                .build();
    }

    private FirstPartyOption createCopyTextFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.TEXT, ContentType.HIGHLIGHTED_TEXT)
                .setContentTypesToDisableFor(ContentType.LINK_AND_TEXT)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_text)
                .setFeatureNameForMetrics("SharingHubAndroid.CopyTextSelected")
                .setOnClickCallback((view) -> {
                    ClipboardManager clipboard = (ClipboardManager) mActivity.getSystemService(
                            Context.CLIPBOARD_SERVICE);
                    clipboard.setPrimaryClip(
                            ClipData.newPlainText(mShareParams.getTitle(), mShareParams.getText()));
                    Toast.makeText(mActivity, R.string.text_copied, Toast.LENGTH_SHORT).show();
                })
                .build();
    }

    private FirstPartyOption createSendTabToSelfFirstPartyOption() {
        return new FirstPartyOptionBuilder(
                ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.IMAGE)
                .setIcon(R.drawable.send_tab, R.string.send_tab_to_self_share_activity_title)
                .setFeatureNameForMetrics("SharingHubAndroid.SendTabToSelfSelected")
                .setOnClickCallback((view) -> {
                    SendTabToSelfCoordinator sttsCoordinator =
                            new SendTabToSelfCoordinator(mActivity, mUrl, mShareParams.getTitle(),
                                    mBottomSheetController, mSettingsLauncher, mIsSyncEnabled,
                                    mTabProvider.get()
                                            .getWebContents()
                                            .getNavigationController()
                                            .getVisibleEntry()
                                            .getTimestamp());
                    sttsCoordinator.show();
                })
                .build();
    }

    private FirstPartyOption createQrCodeFirstPartyOption() {
        return new FirstPartyOptionBuilder(
                ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.IMAGE)
                .setIcon(R.drawable.qr_code, R.string.qr_code_share_icon_label)
                .setFeatureNameForMetrics("SharingHubAndroid.QRCodeSelected")
                .setOnClickCallback((view) -> {
                    QrCodeCoordinator qrCodeCoordinator = new QrCodeCoordinator(mActivity, mUrl);
                    qrCodeCoordinator.show();
                })
                .build();
    }

    private FirstPartyOption createPrintingFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.LINK_PAGE_VISIBLE)
                .setIcon(R.drawable.sharing_print, R.string.print_share_activity_title)
                .setFeatureNameForMetrics("SharingHubAndroid.PrintSelected")
                .setOnClickCallback((view) -> { mPrintTabCallback.onResult(mTabProvider.get()); })
                .build();
    }

    private FirstPartyOption createHighlightsFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.HIGHLIGHTED_TEXT)
                .setIcon(R.drawable.link, R.string.sharing_highlights)
                .setFeatureNameForMetrics("SharingHubAndroid.LinkToTextSelected")
                .setOnClickCallback((view) -> {
                    LinkToTextCoordinator linkToTextCoordinator =
                            new LinkToTextCoordinator(mActivity, mTabProvider.get(),
                                    mChromeOptionShareCallback, mUrl, mShareParams.getText());
                })
                .build();
    }

    static void recordTimeToShare(long shareStartTime) {
        RecordHistogram.recordMediumTimesHistogram("Sharing.SharingHubAndroid.TimeToShare",
                System.currentTimeMillis() - shareStartTime);
    }
}
