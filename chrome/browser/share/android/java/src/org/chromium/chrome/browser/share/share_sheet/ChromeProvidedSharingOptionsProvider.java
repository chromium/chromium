// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.ComponentName;
import android.content.Context;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.content_creation.notes.NoteCreationCoordinator;
import org.chromium.chrome.browser.content_creation.notes.NoteCreationCoordinatorFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.SaveBitmapDelegate;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.share.long_screenshots.LongScreenshotsCoordinator;
import org.chromium.chrome.browser.share.qrcode.QrCodeCoordinator;
import org.chromium.chrome.browser.share.screenshot.ScreenshotCoordinator;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleMetricsHelper.LinkToggleMetricsDetails;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetPropertyModelBuilder.ContentType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Optional;
import java.util.Set;

/**
 * Provides {@code PropertyModel}s of Chrome-provided sharing options.
 */
public class ChromeProvidedSharingOptionsProvider {
    // ComponentName used for Chrome share options in ShareParams.TargetChosenCallback
    public static final ComponentName CHROME_PROVIDED_FEATURE_COMPONENT_NAME =
            new ComponentName("CHROME", "CHROME_FEATURE");

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final Supplier<Tab> mTabProvider;
    private final BottomSheetController mBottomSheetController;
    private final ShareSheetBottomSheetContent mBottomSheetContent;
    private final ShareParams mShareParams;
    private final Callback<Tab> mPrintTabCallback;
    private final boolean mIsIncognito;
    private final long mShareStartTime;
    private final List<FirstPartyOption> mOrderedFirstPartyOptions;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private final String mUrl;
    private final ImageEditorModuleProvider mImageEditorModuleProvider;
    private final Tracker mFeatureEngagementTracker;
    private final @LinkGeneration int mLinkGenerationStatusForMetrics;
    private final LinkToggleMetricsDetails mLinkToggleMetricsDetails;
    private final Profile mProfile;

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
     * * @param profile The current profile of the User.
     */
    ChromeProvidedSharingOptionsProvider(Activity activity, WindowAndroid windowAndroid,
            Supplier<Tab> tabProvider, BottomSheetController bottomSheetController,
            ShareSheetBottomSheetContent bottomSheetContent, ShareParams shareParams,
            Callback<Tab> printTab, boolean isIncognito, long shareStartTime,
            ChromeOptionShareCallback chromeOptionShareCallback,
            ImageEditorModuleProvider imageEditorModuleProvider, Tracker featureEngagementTracker,
            String url, @LinkGeneration int linkGenerationStatusForMetrics,
            LinkToggleMetricsDetails linkToggleMetricsDetails, Profile profile) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mTabProvider = tabProvider;
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mShareParams = shareParams;
        mPrintTabCallback = printTab;
        mIsIncognito = isIncognito;
        mShareStartTime = shareStartTime;
        mImageEditorModuleProvider = imageEditorModuleProvider;
        mFeatureEngagementTracker = featureEngagementTracker;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mUrl = url;
        mLinkGenerationStatusForMetrics = linkGenerationStatusForMetrics;
        mLinkToggleMetricsDetails = linkToggleMetricsDetails;
        mProfile = profile;
        mOrderedFirstPartyOptions = new ArrayList<>();
        initializeFirstPartyOptionsInOrder();
    }

    /**
     * Encapsulates a {@link PropertyModel} and the {@link ContentType}s it should be shown for.
     */
    private static class FirstPartyOption {
        final Collection<Integer> mContentTypes;
        final Collection<Integer> mContentTypesToDisableFor;
        final Collection<Integer> mDetailedContentTypesToDisableFor;
        final PropertyModel mPropertyModel;
        final boolean mDisableForMultiWindow;

        /**
         * Should only be used when the default property model constructed in the builder does not
         * fit the feature's needs. This should be rare.
         *
         * @param model Property model for the first party option.
         * @param contentTypes Content types to trigger for.
         * @param contentTypesToDisableFor Content types to disable for.
         * @param detailedContentTypesToDisableFor {@link DetailedContentType}s to disable for.
         * @param disableForMultiWindow If the feature should be disabled if in multi-window mode.
         */
        FirstPartyOption(PropertyModel model, Collection<Integer> contentTypes,
                Collection<Integer> contentTypesToDisableFor,
                Collection<Integer> detailedContentTypesToDisableFor,
                boolean disableForMultiWindow) {
            mPropertyModel = model;
            mContentTypes = contentTypes;
            mContentTypesToDisableFor = contentTypesToDisableFor;
            mDetailedContentTypesToDisableFor = detailedContentTypesToDisableFor;
            mDisableForMultiWindow = disableForMultiWindow;
        }
    }

    private class FirstPartyOptionBuilder {
        private int mIcon;
        private int mIconLabel;
        private String mIconContentDescription;
        private String mFeatureNameForMetrics;
        private Callback<View> mOnClickCallback;
        private boolean mDisableForMultiWindow;
        private Integer[] mContentTypesToDisableFor;
        private Integer[] mDetailedContentTypesToDisableFor;
        private final Integer[] mContentTypesInBuilder;
        private boolean mShowNewBadge;
        private boolean mHideBottomSheetContentOnTap = true;

        FirstPartyOptionBuilder(Integer... contentTypes) {
            mContentTypesInBuilder = contentTypes;
            mContentTypesToDisableFor = new Integer[] {};
            mDetailedContentTypesToDisableFor = new Integer[] {};
        }

        FirstPartyOptionBuilder setIcon(int icon, int iconLabel) {
            mIcon = icon;
            mIconLabel = iconLabel;
            return this;
        }

        FirstPartyOptionBuilder setIconContentDescription(int iconContentDescription) {
            mIconContentDescription = mActivity.getResources().getString(iconContentDescription);
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
            mContentTypesToDisableFor = contentTypesToDisableFor;
            return this;
        }

        FirstPartyOptionBuilder setDetailedContentTypesToDisableFor(
                Integer... detailedContentTypesToDisableFor) {
            mDetailedContentTypesToDisableFor = detailedContentTypesToDisableFor;
            return this;
        }

        FirstPartyOptionBuilder setDisableForMultiWindow(boolean disableForMultiWindow) {
            mDisableForMultiWindow = disableForMultiWindow;
            return this;
        }

        FirstPartyOptionBuilder setShowNewBadge(boolean showNewBadge) {
            mShowNewBadge = showNewBadge;
            return this;
        }

        FirstPartyOptionBuilder setHideBottomSheetContentOnTap(
                boolean hideBottomSheetContentOnTap) {
            mHideBottomSheetContentOnTap = hideBottomSheetContentOnTap;
            return this;
        }

        FirstPartyOption build() {
            PropertyModel model = ShareSheetPropertyModelBuilder.createPropertyModel(
                    AppCompatResources.getDrawable(mActivity, mIcon),
                    mActivity.getResources().getString(mIconLabel),
                    mIconContentDescription, (view) -> {
                        ShareSheetCoordinator.recordShareMetrics(mFeatureNameForMetrics,
                                mLinkGenerationStatusForMetrics, mLinkToggleMetricsDetails,
                                mShareStartTime, mProfile);
                        if (mHideBottomSheetContentOnTap) {
                            mBottomSheetController.hideContent(mBottomSheetContent, true);
                        }
                        mOnClickCallback.onResult(view);
                        callTargetChosenCallback();
                    }, mShowNewBadge);
            return new FirstPartyOption(model, Arrays.asList(mContentTypesInBuilder),
                    Arrays.asList(mContentTypesToDisableFor),
                    Arrays.asList(mDetailedContentTypesToDisableFor), mDisableForMultiWindow);
        }
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
        for (FirstPartyOption firstPartyOption : mOrderedFirstPartyOptions) {
            if (!Collections.disjoint(contentTypes, firstPartyOption.mContentTypes)
                    && Collections.disjoint(
                            contentTypes, firstPartyOption.mContentTypesToDisableFor)
                    && !firstPartyOption.mDetailedContentTypesToDisableFor.contains(
                            detailedContentType)
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
        boolean enableAllUpcomingSharingFeatures =
                ChromeFeatureList.isEnabled(ChromeFeatureList.UPCOMING_SHARING_FEATURES);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEBNOTES_STYLIZE)) {
            mOrderedFirstPartyOptions.add(createWebNotesStylizeFirstPartyOption());
        }
        mOrderedFirstPartyOptions.add(createScreenshotFirstPartyOption());
        // TODO(crbug.com/1250871): Long Screenshots on by default; supported on Android 7.0+.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT)
                && mTabProvider.hasValue()) {
            mOrderedFirstPartyOptions.add(createLongScreenshotsFirstPartyOption());
        }
        mOrderedFirstPartyOptions.add(createCopyLinkFirstPartyOption());
        mOrderedFirstPartyOptions.add(createCopyGifFirstPartyOption());
        mOrderedFirstPartyOptions.add(createCopyImageFirstPartyOption());
        mOrderedFirstPartyOptions.add(createCopyFirstPartyOption());
        mOrderedFirstPartyOptions.add(createCopyTextFirstPartyOption());
        Optional<Integer> sendTabToSelfDisplayReason =
                SendTabToSelfAndroidBridge.getEntryPointDisplayReason(mProfile, mUrl);
        if (sendTabToSelfDisplayReason.isPresent()
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.SEND_TAB_TO_SELF_SIGNIN_PROMO)) {
            mOrderedFirstPartyOptions.add(createSendTabToSelfFirstPartyOption());
        }
        if (!mIsIncognito) {
            mOrderedFirstPartyOptions.add(createQrCodeFirstPartyOption());
        }
        if (mTabProvider.hasValue() && UserPrefs.get(mProfile).getBoolean(Pref.PRINTING_ENABLED)) {
            mOrderedFirstPartyOptions.add(createPrintingFirstPartyOption());
        }
        mOrderedFirstPartyOptions.add(createSaveImageFirstPartyOption());
    }

    private FirstPartyOption createScreenshotFirstPartyOption() {
        boolean showNewBadge = mFeatureEngagementTracker.isInitialized()
                && mFeatureEngagementTracker.shouldTriggerHelpUI(
                        FeatureConstants.IPH_SHARE_SCREENSHOT_FEATURE);
        return new FirstPartyOptionBuilder(ContentType.LINK_PAGE_VISIBLE, ContentType.TEXT,
                ContentType.HIGHLIGHTED_TEXT, ContentType.IMAGE)
                .setDetailedContentTypesToDisableFor(DetailedContentType.WEB_NOTES)
                .setIcon(R.drawable.screenshot, R.string.sharing_screenshot)
                .setFeatureNameForMetrics("SharingHubAndroid.ScreenshotSelected")
                .setDisableForMultiWindow(true)
                .setShowNewBadge(showNewBadge)
                .setHideBottomSheetContentOnTap(false)
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

    private FirstPartyOption createLongScreenshotsFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.LINK_PAGE_VISIBLE, ContentType.TEXT,
                ContentType.HIGHLIGHTED_TEXT, ContentType.IMAGE)
                .setDetailedContentTypesToDisableFor(DetailedContentType.WEB_NOTES)
                .setIcon(R.drawable.long_screenshot, R.string.sharing_long_screenshot)
                .setFeatureNameForMetrics("SharingHubAndroid.LongScreenshotSelected")
                .setDisableForMultiWindow(true)
                .setHideBottomSheetContentOnTap(false)
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

    private FirstPartyOption createCopyGifFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.IMAGE, ContentType.IMAGE_AND_LINK)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_gif)
                // Enables only for GIF.
                .setDetailedContentTypesToDisableFor(DetailedContentType.IMAGE,
                        DetailedContentType.WEB_NOTES, DetailedContentType.NOT_SPECIFIED)
                .setFeatureNameForMetrics("SharingHubAndroid.CopyGifSelected")
                .setOnClickCallback((view) -> {
                    if (!mShareParams.getFileUris().isEmpty()) {
                        Clipboard.getInstance().setImageUri(mShareParams.getFileUris().get(0));
                        Toast.makeText(mActivity, R.string.gif_copied, Toast.LENGTH_SHORT).show();
                    }
                })
                .build();
    }

    private FirstPartyOption createCopyImageFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.IMAGE, ContentType.IMAGE_AND_LINK)
                .setIcon(R.drawable.ic_content_copy_black, R.string.sharing_copy_image)
                .setFeatureNameForMetrics("SharingHubAndroid.CopyImageSelected")
                .setDetailedContentTypesToDisableFor(DetailedContentType.GIF)
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
                .setDetailedContentTypesToDisableFor(DetailedContentType.WEB_NOTES)
                .setIcon(R.drawable.send_tab, R.string.send_tab_to_self_share_activity_title)
                .setFeatureNameForMetrics("SharingHubAndroid.SendTabToSelfSelected")
                .setOnClickCallback((view) -> {
                    SendTabToSelfCoordinator sttsCoordinator =
                            new SendTabToSelfCoordinator(mActivity, mWindowAndroid, mUrl,
                                    mShareParams.getTitle(), mBottomSheetController, mProfile);
                    sttsCoordinator.show();
                })
                .build();
    }

    private FirstPartyOption createQrCodeFirstPartyOption() {
        return new FirstPartyOptionBuilder(
                ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.IMAGE)
                .setDetailedContentTypesToDisableFor(DetailedContentType.WEB_NOTES)
                .setIcon(R.drawable.qr_code, R.string.qr_code_share_icon_label)
                .setFeatureNameForMetrics("SharingHubAndroid.QRCodeSelected")
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
                .setFeatureNameForMetrics("SharingHubAndroid.PrintSelected")
                .setOnClickCallback((view) -> { mPrintTabCallback.onResult(mTabProvider.get()); })
                .build();
    }

    private FirstPartyOption createWebNotesStylizeFirstPartyOption() {
        boolean showNewBadge = mFeatureEngagementTracker.isInitialized()
                && mFeatureEngagementTracker.shouldTriggerHelpUI(
                        FeatureConstants.SHARING_HUB_WEBNOTES_STYLIZE_FEATURE);
        String title = mShareParams.getTitle();
        return new FirstPartyOptionBuilder(ContentType.HIGHLIGHTED_TEXT)
                .setIcon(R.drawable.webnote, R.string.sharing_webnotes_create_card)
                .setIconContentDescription(R.string.sharing_webnotes_accessibility_description)
                .setFeatureNameForMetrics("SharingHubAndroid.WebnotesStylize")
                .setOnClickCallback((view) -> {
                    mFeatureEngagementTracker.notifyEvent(
                            EventConstants.SHARING_HUB_WEBNOTES_STYLIZE_USED);
                    NoteCreationCoordinator coordinator = NoteCreationCoordinatorFactory.create(
                            mActivity, mShareParams.getWindow(), mUrl, title,
                            mShareParams.getRawText().trim(), mChromeOptionShareCallback);
                    coordinator.showDialog();
                })
                .setShowNewBadge(showNewBadge)
                .build();
    }

    private FirstPartyOption createSaveImageFirstPartyOption() {
        return new FirstPartyOptionBuilder(ContentType.IMAGE, ContentType.IMAGE_AND_LINK)
                .setIcon(R.drawable.save_to_device, R.string.sharing_save_image)
                .setFeatureNameForMetrics("SharingHubAndroid.SaveImageSelected")
                .setOnClickCallback((view) -> {
                    if (mShareParams.getFileUris().isEmpty()) return;

                    ShareImageFileUtils.getBitmapFromUriAsync(
                            mActivity, mShareParams.getFileUris().get(0), (bitmap) -> {
                                SaveBitmapDelegate saveBitmapDelegate = new SaveBitmapDelegate(
                                        mActivity, bitmap, R.string.save_image_filename_prefix,
                                        null, mShareParams.getWindow());
                                saveBitmapDelegate.save();
                            });
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
