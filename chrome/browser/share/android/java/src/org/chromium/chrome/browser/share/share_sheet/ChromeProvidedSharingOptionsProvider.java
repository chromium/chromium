// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.text.TextUtils;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.chrome.browser.share.qrcode.QrCodeCoordinator;
import org.chromium.chrome.browser.share.screenshot.ScreenshotCoordinator;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;

/**
 * Provides {@code PropertyModel}s of Chrome-provided sharing options.
 */
class ChromeProvidedSharingOptionsProvider {
    private Activity mActivity;
    private final Supplier<Tab> mTabProvider;
    private final BottomSheetController mBottomSheetController;
    private final ShareSheetBottomSheetContent mBottomSheetContent;
    private ShareParams mShareParams;
    private ChromeShareExtras mChromeShareExtras;
    private Callback<Tab> mPrintTabCallback;
    private SettingsLauncher mSettingsLauncher;
    private boolean mIsSyncEnabled;
    private long mShareStartTime;
    private ChromeOptionShareCallback mChromeOptionShareCallback;
    private ScreenshotCoordinator mScreenshotCoordinator;
    private String mUrl;
    private boolean mIsMultiWindow;
    private Collection<Integer> mContentTypes;

    /**
     * Constructs a new {@link ChromeProvidedSharingOptionsProvider}.
     *
     * @param tabProvider Supplier for the current activity tab.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param bottomSheetContent The {@link ShareSheetBottomSheetContent} for the current activity.

     */
    ChromeProvidedSharingOptionsProvider(Supplier<Tab> tabProvider,
            BottomSheetController bottomSheetController,
            ShareSheetBottomSheetContent bottomSheetContent) {
        mTabProvider = tabProvider;
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
    }

    /**
     * Sets params related to the share itself.
     *
     * @param shareParams The {@link ShareParams} for the current share.
     * @param chromeShareExtras The {@link ChromeShareExtras} for the current share.
     * @param shareStartTime The start time of the current share.
     * @param chromeOptionShareCallback A ChromeOptionShareCallback that can be used by
     *            Chrome-provided sharing options.
     * @param contentTypes The contentTypes the share is comprised of.
     */
    void setShareRelatedParams(ShareParams shareParams, ChromeShareExtras chromeShareExtras,
            long shareStartTime, ChromeOptionShareCallback chromeOptionShareCallback,
            Collection<Integer> contentTypes) {
        mShareParams = shareParams;
        mShareStartTime = shareStartTime;
        mChromeShareExtras = chromeShareExtras;
        mActivity = mShareParams.getWindow().getActivity().get();
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mContentTypes = contentTypes;
    }

    /**
     * Set params needed by the individual sharing features.
     *
     * @param printTab A {@link Callback} that will print a given Tab.
     * @param settingsLauncher Launches Chrome settings.
     * @param isSyncEnabled Whether the user has enabled sync.
     */
    void setFeatureSpecificParams(
            Callback<Tab> printTab, SettingsLauncher settingsLauncher, boolean isSyncEnabled) {
        mPrintTabCallback = printTab;
        mSettingsLauncher = settingsLauncher;
        mIsSyncEnabled = isSyncEnabled;
    }

    /**
     * Set whether Chrome is in multiWindow mode.
     */
    void setIsMultiWindow(boolean isMultiWindow) {
        mIsMultiWindow = isMultiWindow;
    }

    /**
     * Calculates the property models to display for the Share.
     *
     * <p>New share features should add a call to this function. The order of the functions
     * determine the order in which they appear in the share sheet. Each feature is gated on the
     * supported {@link ContentType}(s).
     *
     * @return the set of ChromeProvidedSharingOptions for the share.
     */
    List<PropertyModel> calculatePropertyModels() {
        mUrl = getUrlToShare(mShareParams, mChromeShareExtras,
                mTabProvider.get().isInitialized() ? mTabProvider.get().getUrl().getSpec() : "");

        List<PropertyModel> propertyModels = new ArrayList<>();

        if (mContentTypes == null || mContentTypes.isEmpty()) {
            return propertyModels;
        }

        maybeAddScreenshotOption(propertyModels);

        maybeAddCopyLinkOption(propertyModels);
        maybeCopyImageOption(propertyModels);
        maybeAddCopyTextAndLinkOption(propertyModels);
        maybeAddCopyTextOption(propertyModels);

        maybeAddSendTabToSelfOption(propertyModels);
        maybeAddHighlightsOption(propertyModels);
        maybeAddQrCodeOption(propertyModels);
        maybeAddPrintingOption(propertyModels);
        return propertyModels;
    }

    private class PropertyModelBuilder {
        private int mIcon;
        private int mIconLabel;
        private String mFeatureNameForMetrics;
        private Callback<View> mOnClickCallback;

        PropertyModelBuilder setIcon(int icon, int iconLabel) {
            mIcon = icon;
            mIconLabel = iconLabel;
            return this;
        }

        PropertyModelBuilder setFeatureNameForMetrics(String featureName) {
            mFeatureNameForMetrics = featureName;
            return this;
        }

        PropertyModelBuilder setOnClickCallback(Callback<View> onClickCallback) {
            mOnClickCallback = onClickCallback;
            return this;
        }

        PropertyModel build() {
            return ShareSheetPropertyModelBuilder.createPropertyModel(
                    AppCompatResources.getDrawable(mActivity, mIcon),
                    mActivity.getResources().getString(mIconLabel), (view) -> {
                        RecordUserAction.record(mFeatureNameForMetrics);
                        recordTimeToShare(mShareStartTime);
                        mBottomSheetController.hideContent(mBottomSheetContent, true);
                        mOnClickCallback.onResult(view);
                    });
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

    private void maybeAddScreenshotOption(List<PropertyModel> propertyModels) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_SCREENSHOT)
                || mIsMultiWindow) {
            return;
        }
        if (Collections.disjoint(mContentTypes,
                    Arrays.asList(ContentType.LINK_PAGE_VISIBLE, ContentType.TEXT,
                            ContentType.HIGHLIGHTED_TEXT, ContentType.IMAGE))) {
            return;
        }

        propertyModels.add(ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(mActivity, R.drawable.screenshot),
                mActivity.getResources().getString(R.string.sharing_screenshot), (view) -> {
                    RecordUserAction.record("SharingHubAndroid.ScreenshotSelected");
                    recordTimeToShare(mShareStartTime);
                    mScreenshotCoordinator = new ScreenshotCoordinator(mActivity,
                            mTabProvider.get(), mChromeOptionShareCallback, mBottomSheetController);
                    // Capture a screenshot once the bottom sheet is fully hidden. The
                    // observer will then remove itself.
                    mBottomSheetController.addObserver(mSheetObserver);
                    mBottomSheetController.hideContent(mBottomSheetContent, true);
                }));
    }

    private void maybeAddCopyLinkOption(List<PropertyModel> propertyModels) {
        if (mContentTypes.contains(ContentType.LINK_AND_TEXT)
                || Collections.disjoint(mContentTypes,
                        Arrays.asList(ContentType.LINK_PAGE_VISIBLE,
                                ContentType.LINK_PAGE_NOT_VISIBLE))) {
            return;
        }

        propertyModels.add(
                new PropertyModelBuilder()
                        .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_url)
                        .setFeatureNameForMetrics("SharingHubAndroid.CopyURLSelected")
                        .setOnClickCallback((view) -> {
                            ClipboardManager clipboard =
                                    (ClipboardManager) mActivity.getSystemService(
                                            Context.CLIPBOARD_SERVICE);
                            clipboard.setPrimaryClip(ClipData.newPlainText(
                                    mShareParams.getTitle(), mShareParams.getUrl()));
                            Toast.makeText(mActivity, R.string.link_copied, Toast.LENGTH_SHORT)
                                    .show();
                        })
                        .build());
    }

    private void maybeCopyImageOption(List<PropertyModel> propertyModels) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)
                || !mContentTypes.contains(ContentType.IMAGE)) {
            return;
        }
        propertyModels.add(
                new PropertyModelBuilder()
                        .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_image)
                        .setFeatureNameForMetrics("SharingHubAndroid.CopyImageSelected")
                        .setOnClickCallback((view) -> {
                            if (!mShareParams.getFileUris().isEmpty()) {
                                Clipboard.getInstance().setImageUri(
                                        mShareParams.getFileUris().get(0));
                                Toast.makeText(mActivity, R.string.image_copied, Toast.LENGTH_SHORT)
                                        .show();
                            }
                        })
                        .build());
    }

    private void maybeAddCopyTextAndLinkOption(List<PropertyModel> propertyModels) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)
                || !mContentTypes.contains(ContentType.LINK_AND_TEXT)) {
            return;
        }
        propertyModels.add(
                new PropertyModelBuilder()
                        .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy)
                        .setFeatureNameForMetrics("SharingHubAndroid.CopySelected")
                        .setOnClickCallback((view) -> {
                            ClipboardManager clipboard =
                                    (ClipboardManager) mActivity.getSystemService(
                                            Context.CLIPBOARD_SERVICE);
                            clipboard.setPrimaryClip(ClipData.newPlainText(
                                    mShareParams.getTitle(), mShareParams.getTextAndUrl()));
                            Toast.makeText(mActivity, R.string.sharing_copied, Toast.LENGTH_SHORT)
                                    .show();
                        })
                        .build());
    }

    private void maybeAddCopyTextOption(List<PropertyModel> propertyModels) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)
                || mContentTypes.contains(ContentType.LINK_AND_TEXT)) {
            return;
        }
        if (Collections.disjoint(
                    mContentTypes, Arrays.asList(ContentType.TEXT, ContentType.HIGHLIGHTED_TEXT))) {
            return;
        }
        propertyModels.add(
                new PropertyModelBuilder()
                        .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_text)
                        .setFeatureNameForMetrics("SharingHubAndroid.CopyTextSelected")
                        .setOnClickCallback((view) -> {
                            ClipboardManager clipboard =
                                    (ClipboardManager) mActivity.getSystemService(
                                            Context.CLIPBOARD_SERVICE);
                            clipboard.setPrimaryClip(ClipData.newPlainText(
                                    mShareParams.getTitle(), mShareParams.getText()));
                            Toast.makeText(mActivity, R.string.text_copied, Toast.LENGTH_SHORT)
                                    .show();
                        })
                        .build());
    }

    private void maybeAddSendTabToSelfOption(List<PropertyModel> propertyModels) {
        if (Collections.disjoint(mContentTypes,
                    Arrays.asList(ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE,
                            ContentType.IMAGE))) {
            return;
        }
        propertyModels.add(
                new PropertyModelBuilder()
                        .setIcon(
                                R.drawable.send_tab, R.string.send_tab_to_self_share_activity_title)
                        .setFeatureNameForMetrics("SharingHubAndroid.SendTabToSelfSelected")
                        .setOnClickCallback((view) -> {
                            SendTabToSelfCoordinator sttsCoordinator = new SendTabToSelfCoordinator(
                                    mActivity, mUrl, mShareParams.getTitle(),
                                    mBottomSheetController, mSettingsLauncher, mIsSyncEnabled,
                                    mTabProvider.get()
                                            .getWebContents()
                                            .getNavigationController()
                                            .getVisibleEntry()
                                            .getTimestamp());
                            sttsCoordinator.show();
                        })
                        .build());
    }

    private void maybeAddQrCodeOption(List<PropertyModel> propertyModels) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_QRCODE)
                || mTabProvider.get().getWebContents().isIncognito()) {
            return;
        }
        if (Collections.disjoint(mContentTypes,
                    Arrays.asList(ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE,
                            ContentType.IMAGE))) {
            return;
        }
        propertyModels.add(new PropertyModelBuilder()
                                   .setIcon(R.drawable.qr_code, R.string.qr_code_share_icon_label)
                                   .setFeatureNameForMetrics("SharingHubAndroid.QRCodeSelected")
                                   .setOnClickCallback((view) -> {
                                       QrCodeCoordinator qrCodeCoordinator =
                                               new QrCodeCoordinator(mActivity, mUrl);
                                       qrCodeCoordinator.show();
                                   })
                                   .build());
    }

    private void maybeAddPrintingOption(List<PropertyModel> propertyModels) {
        if (!UserPrefs.get(Profile.getLastUsedRegularProfile()).getBoolean(Pref.PRINTING_ENABLED)
                || !mContentTypes.contains(ContentType.LINK_PAGE_VISIBLE)) {
            return;
        }
        propertyModels.add(
                new PropertyModelBuilder()
                        .setIcon(R.drawable.sharing_print, R.string.print_share_activity_title)
                        .setFeatureNameForMetrics("SharingHubAndroid.PrintSelected")
                        .setOnClickCallback(
                                (view) -> { mPrintTabCallback.onResult(mTabProvider.get()); })
                        .build());
    }

    private void maybeAddHighlightsOption(List<PropertyModel> propertyModels) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_HIGHLIGHTS_ANDROID)
                || !mContentTypes.contains(ContentType.HIGHLIGHTED_TEXT)) {
            return;
        }
        propertyModels.add(
                new PropertyModelBuilder()
                        .setIcon(R.drawable.link, R.string.sharing_highlights)
                        .setFeatureNameForMetrics("SharingHubAndroid.LinkToTextSelected")
                        .setOnClickCallback((view) -> {
                            LinkToTextCoordinator linkToTextCoordinator = new LinkToTextCoordinator(
                                    mActivity, mTabProvider.get(), mChromeOptionShareCallback, mUrl,
                                    mShareParams.getText());
                        })
                        .build());
    }

    /**
     * Returns the url to share.
     *
     * <p> This prioritizes the URL in {@link ShareParams}, but if it does not exist, we look for an
     * image source URL from {@link ChromeShareExtras}. The image source URL is not contained in
     * {@link ShareParams#getUrl()} because we do not want to share the image URL with the image
     * file in third-party app shares. If both are empty then current tab URL is used. This is
     * useful for {@link LinkToTextCoordinator} that needs URL but it cannot be provided through
     * {@link ShareParams}.
     */
    static String getUrlToShare(
            ShareParams shareParams, ChromeShareExtras chromeShareExtras, String tabUrl) {
        if (!TextUtils.isEmpty(shareParams.getUrl())) {
            return shareParams.getUrl();
        } else if (!TextUtils.isEmpty(chromeShareExtras.getImageSrcUrl())) {
            return chromeShareExtras.getImageSrcUrl();
        }
        return tabUrl;
    }

    static void recordTimeToShare(long shareStartTime) {
        RecordHistogram.recordMediumTimesHistogram("Sharing.SharingHubAndroid.TimeToShare",
                System.currentTimeMillis() - shareStartTime);
    }
}
