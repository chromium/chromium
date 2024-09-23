// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.net.Uri;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareContentTypeHelper.ContentType;
import org.chromium.chrome.browser.share.ShareMetricsUtils.ShareCustomAction;
import org.chromium.chrome.browser.share.qrcode.QrCodeCoordinator;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Optional;
import java.util.Set;

/** Provides a list of Chrome-provided sharing options. */
public abstract class ChromeProvidedSharingOptionsProviderBase {
    private static final String USER_ACTION_COPY_URL_SELECTED = "SharingHubAndroid.CopyURLSelected";
    private static final String USER_ACTION_COPY_IMAGE_SELECTED =
            "SharingHubAndroid.CopyImageSelected";
    private static final String USER_ACTION_COPY_SELECTED = "SharingHubAndroid.CopySelected";
    private static final String USER_ACTION_COPY_TEXT_SELECTED =
            "SharingHubAndroid.CopyTextSelected";
    private static final String USER_ACTION_SEND_TAB_TO_SELF_SELECTED =
            "SharingHubAndroid.SendTabToSelfSelected";
    private static final String USER_ACTION_QR_CODE_SELECTED = "SharingHubAndroid.QRCodeSelected";
    private static final String USER_ACTION_PRINT_SELECTED = "SharingHubAndroid.PrintSelected";
    protected static final String USER_ACTION_LONG_SCREENSHOT_SELECTED =
            "SharingHubAndroid.LongScreenshotSelected";

    protected final Activity mActivity;
    protected final WindowAndroid mWindowAndroid;
    protected final Supplier<Tab> mTabProvider;
    protected final BottomSheetController mBottomSheetController;
    protected final ShareParams mShareParams;
    protected final Callback<Tab> mPrintTabCallback;
    protected final boolean mIsIncognito;
    protected final List<FirstPartyOption> mOrderedFirstPartyOptions;
    protected final ChromeOptionShareCallback mChromeOptionShareCallback;
    protected final String mUrl;
    protected final Tracker mFeatureEngagementTracker;
    protected final Profile mProfile;
    protected final DeviceLockActivityLauncher mDeviceLockActivityLauncher;

    /**
     * Constructs a new {@link ChromeProvidedSharingOptionsProviderBase}.
     *
     * @param activity The current {@link Activity}.
     * @param windowAndroid The current window.
     * @param tabProvider Supplier for the current activity tab.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param shareParams The {@link ShareParams} for the current share.
     * @param printTab A {@link Callback} that will print a given Tab.
     * @param isIncognito Whether incognito mode is enabled.
     * @param chromeOptionShareCallback A ChromeOptionShareCallback that can be used by
     * Chrome-provided sharing options.
     * @param featureEngagementTracker feature engagement tracker.
     * @param url Url to share.
     * @param profile The current profile of the User.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     */
    protected ChromeProvidedSharingOptionsProviderBase(
            Activity activity,
            WindowAndroid windowAndroid,
            Supplier<Tab> tabProvider,
            BottomSheetController bottomSheetController,
            ShareParams shareParams,
            Callback<Tab> printTab,
            boolean isIncognito,
            ChromeOptionShareCallback chromeOptionShareCallback,
            Tracker featureEngagementTracker,
            String url,
            Profile profile,
            DeviceLockActivityLauncher deviceLockActivityLauncher) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mTabProvider = tabProvider;
        mBottomSheetController = bottomSheetController;
        mShareParams = shareParams;
        mPrintTabCallback = printTab;
        mIsIncognito = isIncognito;
        mFeatureEngagementTracker = featureEngagementTracker;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mUrl = url;
        mProfile = profile;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;

        mOrderedFirstPartyOptions = new ArrayList<>();
    }

    /** Data structure carries details on how a first party option should be used. */
    protected static class FirstPartyOption {
        public final @ShareCustomAction int shareActionType;
        public final int icon;
        public final int iconLabel;
        public final String iconContentDescription;
        public final String featureNameForMetrics;
        public final Callback<View> onClickCallback;
        public final Collection<Integer> contentTypes;
        public final Collection<Integer> contentTypesToDisableFor;
        public final Collection<Integer> detailedContentTypesToDisableFor;
        public final boolean disableForMultiWindow;

        private FirstPartyOption(
                @ShareCustomAction int shareActionType,
                int icon,
                int iconLabel,
                String iconContentDescription,
                String featureNameForMetrics,
                Callback<View> onClickCallback,
                Collection<Integer> contentTypes,
                Collection<Integer> contentTypesToDisableFor,
                Collection<Integer> detailedContentTypesToDisableFor,
                boolean disableForMultiWindow) {
            this.shareActionType = shareActionType;
            this.icon = icon;
            this.iconLabel = iconLabel;
            this.iconContentDescription = iconContentDescription;
            this.featureNameForMetrics = featureNameForMetrics;
            this.onClickCallback = onClickCallback;
            this.contentTypes = contentTypes;
            this.contentTypesToDisableFor = contentTypesToDisableFor;
            this.detailedContentTypesToDisableFor = detailedContentTypesToDisableFor;
            this.disableForMultiWindow = disableForMultiWindow;
        }
    }

    protected static class FirstPartyOptionBuilder {
        private int mIcon;
        private int mIconLabel;
        private String mIconContentDescription;
        // Use a default invalid enum, forcing client to set it.
        private @ShareCustomAction int mShareActionType = ShareCustomAction.NUM_ENTRIES;
        private String mFeatureNameForMetrics;
        private Callback<View> mOnClickCallback;
        private boolean mDisableForMultiWindow;
        private Integer[] mContentTypesToDisableFor;
        private Integer[] mDetailedContentTypesToDisableFor;
        private final Integer[] mContentTypesInBuilder;

        public FirstPartyOptionBuilder(Integer... contentTypes) {
            mContentTypesInBuilder = contentTypes;
            mContentTypesToDisableFor = new Integer[] {};
            mDetailedContentTypesToDisableFor = new Integer[] {};
        }

        public FirstPartyOptionBuilder setShareActionType(@ShareCustomAction int shareActionType) {
            mShareActionType = shareActionType;
            return this;
        }

        public FirstPartyOptionBuilder setIcon(int icon, int iconLabel) {
            mIcon = icon;
            mIconLabel = iconLabel;
            return this;
        }

        public FirstPartyOptionBuilder setIconContentDescription(String iconContentDescription) {
            mIconContentDescription = iconContentDescription;
            return this;
        }

        public FirstPartyOptionBuilder setFeatureNameForMetrics(String featureName) {
            mFeatureNameForMetrics = featureName;
            return this;
        }

        public FirstPartyOptionBuilder setOnClickCallback(Callback<View> onClickCallback) {
            mOnClickCallback = onClickCallback;
            return this;
        }

        public FirstPartyOptionBuilder setContentTypesToDisableFor(
                Integer... contentTypesToDisableFor) {
            mContentTypesToDisableFor = contentTypesToDisableFor;
            return this;
        }

        public FirstPartyOptionBuilder setDetailedContentTypesToDisableFor(
                Integer... detailedContentTypesToDisableFor) {
            mDetailedContentTypesToDisableFor = detailedContentTypesToDisableFor;
            return this;
        }

        public FirstPartyOptionBuilder setDisableForMultiWindow(boolean disableForMultiWindow) {
            mDisableForMultiWindow = disableForMultiWindow;
            return this;
        }

        public FirstPartyOption build() {
            assert mShareActionType != ShareCustomAction.NUM_ENTRIES : "ShareActionType not set.";
            assert mOnClickCallback != null;
            return new FirstPartyOption(
                    mShareActionType,
                    mIcon,
                    mIconLabel,
                    mIconContentDescription,
                    mFeatureNameForMetrics,
                    mOnClickCallback,
                    Arrays.asList(mContentTypesInBuilder),
                    Arrays.asList(mContentTypesToDisableFor),
                    Arrays.asList(mDetailedContentTypesToDisableFor),
                    mDisableForMultiWindow);
        }
    }

    /**
     * Returns an ordered list of {@link FirstPartyOption}s that should be shown given the {@code
     * contentTypes} being shared.
     *
     * @param contentTypes a {@link Set} of {@link ContentType}.
     * @param detailedContentType the {@link DetailedContentType} being shared.
     * @param isMultiWindow if in multi-window mode.
     * @return a list of {@link FirstPartyOption}s.
     */
    protected List<FirstPartyOption> getFirstPartyOptions(
            @ContentType Set<Integer> contentTypes,
            @DetailedContentType int detailedContentType,
            boolean isMultiWindow) {
        List<FirstPartyOption> availableOptions = new ArrayList<>();
        for (FirstPartyOption firstPartyOption : mOrderedFirstPartyOptions) {
            if (!Collections.disjoint(contentTypes, firstPartyOption.contentTypes)
                    && Collections.disjoint(contentTypes, firstPartyOption.contentTypesToDisableFor)
                    && !firstPartyOption.detailedContentTypesToDisableFor.contains(
                            detailedContentType)
                    && !(isMultiWindow && firstPartyOption.disableForMultiWindow)) {
                availableOptions.add(firstPartyOption);
            }
        }
        return availableOptions;
    }

    /**
     * Creates all enabled {@link FirstPartyOption}s and adds them to {@code
     * mOrderedFirstPartyOptions} in the order they should appear. This has to be called by child
     * classes before the provider can function
     */
    protected void initializeFirstPartyOptionsInOrder() {
        maybeAddPageInfoFirstPartyOption();
        maybeAddCopyFirstPartyOption();
        // Only show a limited first party share selection for automotive and PDF pages.
        if (!isAutomotive() && !isPdfTab()) {
            maybeAddLongScreenshotFirstPartyOption();
            maybeAddPrintFirstPartyOption();
        }
        maybeAddSendTabToSelfFirstPartyOption();
        maybeAddQrCodeFirstPartyOption();
    }

    private void maybeAddPageInfoFirstPartyOption() {
        FirstPartyOption pageInfoOption = createPageInfoFirstPartyOption();
        if (pageInfoOption != null) {
            mOrderedFirstPartyOptions.add(pageInfoOption);
        }
    }

    private void maybeAddSendTabToSelfFirstPartyOption() {
        Optional<Integer> sendTabToSelfDisplayReason =
                SendTabToSelfAndroidBridge.getEntryPointDisplayReason(mProfile, mUrl);
        if (sendTabToSelfDisplayReason.isPresent()) {
            mOrderedFirstPartyOptions.add(createSendTabToSelfFirstPartyOption());
        }
    }

    private void maybeAddQrCodeFirstPartyOption() {
        if (!mIsIncognito && !TextUtils.isEmpty(mUrl)) {
            mOrderedFirstPartyOptions.add(createQrCodeFirstPartyOption());
        }
    }

    private void maybeAddLongScreenshotFirstPartyOption() {
        if (!mTabProvider.hasValue()) {
            return;
        }
        FirstPartyOption option = createLongScreenshotsFirstPartyOption();
        if (option != null) {
            mOrderedFirstPartyOptions.add(option);
        }
    }

    private void maybeAddPrintFirstPartyOption() {
        if (mTabProvider.hasValue() && UserPrefs.get(mProfile).getBoolean(Pref.PRINTING_ENABLED)) {
            mOrderedFirstPartyOptions.add(createPrintingFirstPartyOption());
        }
    }

    protected void maybeAddCopyFirstPartyOption() {
        mOrderedFirstPartyOptions.add(createCopyLinkFirstPartyOption());
        mOrderedFirstPartyOptions.add(createCopyImageFirstPartyOption());
        mOrderedFirstPartyOptions.add(createCopyFirstPartyOption());
        mOrderedFirstPartyOptions.add(createCopyTextFirstPartyOption());
    }

    private boolean isPdfTab() {
        return mTabProvider.hasValue()
                && mTabProvider.get().isNativePage()
                && mTabProvider.get().getNativePage().isPdf();
    }

    private static boolean isAutomotive() {
        return BuildInfo.getInstance().isAutomotive;
    }

    private FirstPartyOption createCopyLinkFirstPartyOption() {
        return new FirstPartyOptionBuilder(
                        ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE)
                .setContentTypesToDisableFor(ContentType.LINK_AND_TEXT, ContentType.IMAGE_AND_LINK)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_url)
                .setShareActionType(ShareCustomAction.COPY_URL)
                .setFeatureNameForMetrics(USER_ACTION_COPY_URL_SELECTED)
                .setOnClickCallback(
                        (view) -> {
                            Clipboard.getInstance()
                                    .setText(
                                            mShareParams.getTitle(),
                                            mShareParams.getUrl(),
                                            /* notifyOnSuccess= */ true);
                        })
                .build();
    }

    /**
     * @return The copy first party option.
     */
    protected FirstPartyOption createCopyImageFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.IMAGE, ContentType.IMAGE_AND_LINK)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_image)
                .setShareActionType(ShareCustomAction.COPY_IMAGE)
                .setFeatureNameForMetrics(USER_ACTION_COPY_IMAGE_SELECTED)
                .setOnClickCallback(
                        (view) -> {
                            Uri imageUri = mShareParams.getImageUriToShare();
                            if (imageUri != null) {
                                Clipboard.getInstance()
                                        .setImageUri(imageUri, /* notifyOnSuccess= */ true);
                            }
                        })
                .build();
    }

    private FirstPartyOption createCopyFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.LINK_AND_TEXT)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy)
                .setShareActionType(ShareCustomAction.COPY)
                .setFeatureNameForMetrics(USER_ACTION_COPY_SELECTED)
                .setOnClickCallback(
                        (view) -> {
                            Clipboard.getInstance()
                                    .setText(
                                            mShareParams.getTitle(),
                                            mShareParams.getTextAndUrl(),
                                            /* notifyOnSuccess= */ true);
                        })
                .build();
    }

    private FirstPartyOption createCopyTextFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.TEXT, ContentType.HIGHLIGHTED_TEXT)
                .setContentTypesToDisableFor(ContentType.LINK_AND_TEXT)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_text)
                .setShareActionType(ShareCustomAction.COPY_TEXT)
                .setFeatureNameForMetrics(USER_ACTION_COPY_TEXT_SELECTED)
                .setOnClickCallback(
                        (view) -> {
                            Clipboard.getInstance()
                                    .setText(
                                            mShareParams.getTitle(),
                                            mShareParams.getText(),
                                            /* notifyOnSuccess= */ true);
                        })
                .build();
    }

    private FirstPartyOption createSendTabToSelfFirstPartyOption() {
        return new FirstPartyOptionBuilder(
                        ContentType.LINK_PAGE_VISIBLE,
                        ContentType.LINK_PAGE_NOT_VISIBLE,
                        ContentType.IMAGE)
                .setDetailedContentTypesToDisableFor(DetailedContentType.SCREENSHOT)
                .setIcon(R.drawable.send_tab, R.string.sharing_send_tab_to_self)
                .setShareActionType(ShareCustomAction.SEND_TAB_TO_SELF)
                .setFeatureNameForMetrics(USER_ACTION_SEND_TAB_TO_SELF_SELECTED)
                .setOnClickCallback(
                        (view) -> {
                            SendTabToSelfCoordinator sttsCoordinator =
                                    new SendTabToSelfCoordinator(
                                            mActivity,
                                            mWindowAndroid,
                                            mUrl,
                                            mShareParams.getTitle(),
                                            mBottomSheetController,
                                            mProfile,
                                            mDeviceLockActivityLauncher);
                            sttsCoordinator.show();
                        })
                .build();
    }

    private FirstPartyOption createQrCodeFirstPartyOption() {
        return new FirstPartyOptionBuilder(
                        ContentType.LINK_PAGE_VISIBLE,
                        ContentType.LINK_PAGE_NOT_VISIBLE,
                        ContentType.IMAGE)
                .setDetailedContentTypesToDisableFor(DetailedContentType.SCREENSHOT)
                .setIcon(R.drawable.qr_code, R.string.qr_code_share_icon_label)
                .setShareActionType(ShareCustomAction.QR_CODE)
                .setFeatureNameForMetrics(USER_ACTION_QR_CODE_SELECTED)
                .setOnClickCallback(
                        (view) -> {
                            QrCodeCoordinator qrCodeCoordinator =
                                    new QrCodeCoordinator(
                                            mActivity, mUrl, mShareParams.getWindow());
                            qrCodeCoordinator.show();
                        })
                .build();
    }

    private FirstPartyOption createPrintingFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.LINK_PAGE_VISIBLE)
                .setIcon(R.drawable.sharing_print, R.string.print_share_activity_title)
                .setShareActionType(ShareCustomAction.PRINT)
                .setFeatureNameForMetrics(USER_ACTION_PRINT_SELECTED)
                .setOnClickCallback(
                        (view) -> {
                            mPrintTabCallback.onResult(mTabProvider.get());
                        })
                .build();
    }

    /**
     * Create a {@link FirstPartyOption} used to do long screenshot. Return null if not supported.
     */
    protected abstract @Nullable FirstPartyOption createLongScreenshotsFirstPartyOption();

    /**
     * Create a {@link FirstPartyOption} used for page info sharing. Return null if not supported.
     */
    protected abstract @Nullable FirstPartyOption createPageInfoFirstPartyOption();
}
