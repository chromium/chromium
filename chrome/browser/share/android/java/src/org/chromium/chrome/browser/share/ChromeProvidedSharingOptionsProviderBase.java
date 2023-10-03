// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.net.Uri;
import android.os.Build;
import android.os.Build.VERSION;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.content_creation.notes.NoteCreationCoordinator;
import org.chromium.chrome.browser.content_creation.notes.NoteCreationCoordinatorFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareContentTypeHelper.ContentType;
import org.chromium.chrome.browser.share.qrcode.QrCodeCoordinator;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Optional;
import java.util.Set;

/**
 * Provides a list of Chrome-provided sharing options.
 */
public abstract class ChromeProvidedSharingOptionsProviderBase {
    private static final String USER_ACTION_COPY_URL_SELECTED = "SharingHubAndroid.CopyURLSelected";
    private static final String USER_ACTION_COPY_GIF_SELECTED = "SharingHubAndroid.CopyGifSelected";
    private static final String USER_ACTION_COPY_IMAGE_SELECTED =
            "SharingHubAndroid.CopyImageSelected";
    private static final String USER_ACTION_COPY_SELECTED = "SharingHubAndroid.CopySelected";
    private static final String USER_ACTION_COPY_TEXT_SELECTED =
            "SharingHubAndroid.CopyTextSelected";
    private static final String USER_ACTION_SEND_TAB_TO_SELF_SELECTED =
            "SharingHubAndroid.SendTabToSelfSelected";
    private static final String USER_ACTION_QR_CODE_SELECTED = "SharingHubAndroid.QRCodeSelected";
    private static final String USER_ACTION_PRINT_SELECTED = "SharingHubAndroid.PrintSelected";
    private static final String USER_ACTION_SAVE_IMAGE_SELECTED =
            "SharingHubAndroid.SaveImageSelected";

    protected static final String USER_ACTION_WEB_STYLE_NOTES_SELECTED =
            "SharingHubAndroid.WebnotesStylize";

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
    protected ChromeProvidedSharingOptionsProviderBase(Activity activity,
            WindowAndroid windowAndroid, Supplier<Tab> tabProvider,
            BottomSheetController bottomSheetController, ShareParams shareParams,
            Callback<Tab> printTab, boolean isIncognito,
            ChromeOptionShareCallback chromeOptionShareCallback, Tracker featureEngagementTracker,
            String url, Profile profile, DeviceLockActivityLauncher deviceLockActivityLauncher) {
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

    /**
     * Data structure carries details on how a first party option should be used.
     */
    protected static class FirstPartyOption {
        public final int icon;
        public final int iconLabel;
        public final String iconContentDescription;
        public final String featureNameForMetrics;
        public final Callback<View> onClickCallback;
        public final Collection<Integer> contentTypes;
        public final Collection<Integer> contentTypesToDisableFor;
        public final Collection<Integer> detailedContentTypesToDisableFor;
        public final boolean disableForMultiWindow;

        private FirstPartyOption(int icon, int iconLabel, String iconContentDescription,
                String featureNameForMetrics, Callback<View> onClickCallback,
                Collection<Integer> contentTypes, Collection<Integer> contentTypesToDisableFor,
                Collection<Integer> detailedContentTypesToDisableFor,
                boolean disableForMultiWindow) {
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
            assert mOnClickCallback != null;
            return new FirstPartyOption(mIcon, mIconLabel, mIconContentDescription,
                    mFeatureNameForMetrics, mOnClickCallback, Arrays.asList(mContentTypesInBuilder),
                    Arrays.asList(mContentTypesToDisableFor),
                    Arrays.asList(mDetailedContentTypesToDisableFor), mDisableForMultiWindow);
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
    protected List<FirstPartyOption> getFirstPartyOptions(@ContentType Set<Integer> contentTypes,
            @DetailedContentType int detailedContentType, boolean isMultiWindow) {
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

    protected boolean usePolishedActionOrderedList() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SHARE_SHEET_CUSTOM_ACTIONS_POLISH);
    }

    /**
     * Creates all enabled {@link FirstPartyOption}s and adds them to {@code
     * mOrderedFirstPartyOptions} in the order they should appear. This has to be called by child
     * classes before the provider can function
     */
    protected void initializeFirstPartyOptionsInOrder() {
        // Only show a limited first party share selection for automotive
        if (BuildInfo.getInstance().isAutomotive) {
            maybeAddCopyFirstPartyOption();
            maybeAddSendTabToSelfFirstPartyOption();
            maybeAddQrCodeFirstPartyOption();
            return;
        }
        if (usePolishedActionOrderedList()) {
            maybeAddCopyFirstPartyOption();
            maybeAddLongScreenshotFirstPartyOption();
            maybeAddPrintFirstPartyOption();
            maybeAddSendTabToSelfFirstPartyOption();
            maybeAddQrCodeFirstPartyOption();
        } else {
            maybeAddWebStyleNotesFirstPartyOption();
            maybeAddScreenshotFirstPartyOption();
            maybeAddLongScreenshotFirstPartyOption();
            // Always show the copy link option as some entries does not offer the change for copy
            // (e.g. feed card)
            maybeAddCopyFirstPartyOption();
            maybeAddSendTabToSelfFirstPartyOption();
            maybeAddQrCodeFirstPartyOption();
            maybeAddPrintFirstPartyOption();
            maybeAddDownloadImageFirstPartyOption();
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

    private void maybeAddScreenshotFirstPartyOption() {
        FirstPartyOption option = createScreenshotFirstPartyOption();
        if (option != null) {
            mOrderedFirstPartyOptions.add(option);
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

    protected void maybeAddWebStyleNotesFirstPartyOption() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEBNOTES_STYLIZE)) {
            mOrderedFirstPartyOptions.add(createWebNotesStylizeFirstPartyOption());
        }
    }

    protected void maybeAddCopyFirstPartyOption() {
        mOrderedFirstPartyOptions.add(createCopyLinkFirstPartyOption());
        if (usePolishedActionOrderedList()) {
            mOrderedFirstPartyOptions.add(createCopyImageFirstPartyOption(false));
        } else {
            mOrderedFirstPartyOptions.add(createCopyGifFirstPartyOption());
            mOrderedFirstPartyOptions.add(createCopyImageFirstPartyOption(true));
        }
        mOrderedFirstPartyOptions.add(createCopyFirstPartyOption());
        mOrderedFirstPartyOptions.add(createCopyTextFirstPartyOption());
    }

    protected void maybeAddDownloadImageFirstPartyOption() {
        mOrderedFirstPartyOptions.add(createSaveImageFirstPartyOption());
    }

    private FirstPartyOption createCopyLinkFirstPartyOption() {
        FirstPartyOptionBuilder builder = new FirstPartyOptionBuilder(
                ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE);
        if (usePolishedActionOrderedList()) {
            builder.setContentTypesToDisableFor(
                    ContentType.LINK_AND_TEXT, ContentType.IMAGE_AND_LINK);
        } else {
            builder.setContentTypesToDisableFor(ContentType.LINK_AND_TEXT);
        }

        return builder.setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_url)
                .setFeatureNameForMetrics(USER_ACTION_COPY_URL_SELECTED)
                .setOnClickCallback((view) -> {
                    Clipboard.getInstance().setText(mShareParams.getTitle(), mShareParams.getUrl(),
                            /*notifyOnSuccess=*/true);
                })
                .build();
    }

    private FirstPartyOption createCopyGifFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.IMAGE, ContentType.IMAGE_AND_LINK)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_gif)
                // Enables only for GIF.
                .setDetailedContentTypesToDisableFor(DetailedContentType.IMAGE,
                        DetailedContentType.WEB_NOTES, DetailedContentType.NOT_SPECIFIED)
                .setFeatureNameForMetrics(USER_ACTION_COPY_GIF_SELECTED)
                .setOnClickCallback((view) -> {
                    Uri imageUri = mShareParams.getImageUriToShare();
                    if (imageUri != null) {
                        Clipboard.getInstance().setImageUri(imageUri);
                        // TODO(crbug/1448589): Remove copy GIF action.
                        // This is separate from regular image copy due to the string used on the
                        // toast. To avoid growing complexity to customize text on toast in
                        // Clipboard, this is logic guarded by version code.
                        if (VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                            Toast.makeText(mActivity, R.string.gif_copied, Toast.LENGTH_SHORT)
                                    .show();
                        }
                    }
                })
                .build();
    }

    /**
     * @param excludeGif Whether exclude the GIF copy from copy image action.
     * @return The copy first party option.
     */
    protected FirstPartyOption createCopyImageFirstPartyOption(boolean excludeGif) {
        FirstPartyOptionBuilder builder =
                new FirstPartyOptionBuilder(ContentType.IMAGE, ContentType.IMAGE_AND_LINK)
                        .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_image)
                        .setFeatureNameForMetrics(USER_ACTION_COPY_IMAGE_SELECTED)
                        .setOnClickCallback((view) -> {
                            Uri imageUri = mShareParams.getImageUriToShare();
                            if (imageUri != null) {
                                Clipboard.getInstance().setImageUri(
                                        imageUri, /*notifyOnSuccess=*/true);
                            }
                        });
        if (excludeGif) {
            builder.setDetailedContentTypesToDisableFor(DetailedContentType.GIF);
        }
        return builder.build();
    }

    private FirstPartyOption createCopyFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.LINK_AND_TEXT)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy)
                .setFeatureNameForMetrics(USER_ACTION_COPY_SELECTED)
                .setOnClickCallback((view) -> {
                    Clipboard.getInstance().setText(mShareParams.getTitle(),
                            mShareParams.getTextAndUrl(), /*notifyOnSuccess=*/true);
                })
                .build();
    }

    private FirstPartyOption createCopyTextFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.TEXT, ContentType.HIGHLIGHTED_TEXT)
                .setContentTypesToDisableFor(ContentType.LINK_AND_TEXT)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_text)
                .setFeatureNameForMetrics(USER_ACTION_COPY_TEXT_SELECTED)
                .setOnClickCallback((view) -> {
                    Clipboard.getInstance().setText(mShareParams.getTitle(), mShareParams.getText(),
                            /*notifyOnSuccess=*/true);
                })
                .build();
    }

    private FirstPartyOption createSendTabToSelfFirstPartyOption() {
        return new FirstPartyOptionBuilder(
                ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.IMAGE)
                .setDetailedContentTypesToDisableFor(
                        DetailedContentType.WEB_NOTES, DetailedContentType.SCREENSHOT)
                .setIcon(R.drawable.send_tab, R.string.sharing_send_tab_to_self)
                .setFeatureNameForMetrics(USER_ACTION_SEND_TAB_TO_SELF_SELECTED)
                .setOnClickCallback((view) -> {
                    SendTabToSelfCoordinator sttsCoordinator = new SendTabToSelfCoordinator(
                            mActivity, mWindowAndroid, mUrl, mShareParams.getTitle(),
                            mBottomSheetController, mProfile, mDeviceLockActivityLauncher);
                    sttsCoordinator.show();
                })
                .build();
    }

    private FirstPartyOption createQrCodeFirstPartyOption() {
        return new FirstPartyOptionBuilder(
                ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.IMAGE)
                .setDetailedContentTypesToDisableFor(
                        DetailedContentType.WEB_NOTES, DetailedContentType.SCREENSHOT)
                .setIcon(R.drawable.qr_code, R.string.qr_code_share_icon_label)
                .setFeatureNameForMetrics(USER_ACTION_QR_CODE_SELECTED)
                .setOnClickCallback((view) -> {
                    QrCodeCoordinator qrCodeCoordinator =
                            new QrCodeCoordinator(mActivity, mUrl, mShareParams.getWindow());
                    qrCodeCoordinator.show();
                })
                .build();
    }

    private FirstPartyOption createPrintingFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.LINK_PAGE_VISIBLE)
                .setIcon(R.drawable.sharing_print, R.string.print_share_activity_title)
                .setFeatureNameForMetrics(USER_ACTION_PRINT_SELECTED)
                .setOnClickCallback((view) -> { mPrintTabCallback.onResult(mTabProvider.get()); })
                .build();
    }

    private FirstPartyOption createSaveImageFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.IMAGE, ContentType.IMAGE_AND_LINK)
                .setIcon(R.drawable.save_to_device, R.string.sharing_save_image)
                .setFeatureNameForMetrics(USER_ACTION_SAVE_IMAGE_SELECTED)
                .setOnClickCallback((view) -> {
                    Uri imageUri = mShareParams.getImageUriToShare();
                    if (imageUri == null) return;
                    ShareImageFileUtils.getBitmapFromUriAsync(
                            mActivity, imageUri, (bitmap) -> {
                                SaveBitmapDelegate saveBitmapDelegate = new SaveBitmapDelegate(
                                        mActivity, bitmap, R.string.save_image_filename_prefix,
                                        null, mShareParams.getWindow());
                                saveBitmapDelegate.save();
                            });
                })
                .build();
    }

    private FirstPartyOption createWebNotesStylizeFirstPartyOption() {
        String title = mShareParams.getTitle();
        return new FirstPartyOptionBuilder(ContentType.HIGHLIGHTED_TEXT)
                .setIcon(R.drawable.webnote, R.string.sharing_webnotes_create_card)
                .setIconContentDescription(
                        mActivity.getString(R.string.sharing_webnotes_accessibility_description))
                .setFeatureNameForMetrics(USER_ACTION_WEB_STYLE_NOTES_SELECTED)
                .setOnClickCallback((view) -> {
                    mFeatureEngagementTracker.notifyEvent(
                            EventConstants.SHARING_HUB_WEBNOTES_STYLIZE_USED);
                    NoteCreationCoordinator coordinator = NoteCreationCoordinatorFactory.create(
                            mActivity, mShareParams.getWindow(), mUrl, title,
                            mShareParams.getRawText().trim(), mChromeOptionShareCallback);
                    coordinator.showDialog();
                })
                .build();
    }

    /**
     * Create a {@link FirstPartyOption} used to do screenshot. Return null if not supported.
     */
    protected abstract @Nullable FirstPartyOption createScreenshotFirstPartyOption();

    /**
     * Create a {@link FirstPartyOption} used to do long screenshot. Return null if not supported.
     */
    protected abstract @Nullable FirstPartyOption createLongScreenshotsFirstPartyOption();
}
