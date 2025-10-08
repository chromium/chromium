// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.DEFAULT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.launchUriActivity;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.IS_SECTION_TRAILING_ICON_VISIBLE;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LEADING_ICON_FOR_THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LEARN_MORE_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SECTION_ON_CLICK_LISTENER;

import android.content.Context;
import android.graphics.Bitmap;
import android.support.annotation.VisibleForTesting;
import android.util.Pair;
import android.view.View;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.ActivityResultRegistry;
import androidx.activity.result.contract.ActivityResultContracts;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsCoordinator;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the NTP appearance settings bottom sheet in the NTP customization. */
@NullMarked
public class NtpThemeMediator {
    @VisibleForTesting static final String UPLOAD_IMAGE_KEY = "NtpThemeUploadImage";

    // TODO(crbug.com/423579377): Update the url for learn more button.
    private static final String LEARN_MORE_CLICK_URL =
            "https://support.google.com/chrome/?p=new_tab";
    private final Profile mProfile;
    private final PropertyModel mBottomSheetPropertyModel;
    private final PropertyModel mThemePropertyModel;
    private final BottomSheetDelegate mBottomSheetDelegate;
    private final Context mContext;
    private final NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    private final Callback<@Nullable Bitmap> mOnImageSelectedCallback;
    private final CallbackController mCallbackController = new CallbackController();
    private @Nullable ActivityResultRegistry mActivityResultRegistry;
    private @Nullable ActivityResultLauncher<String> mActivityResultLauncher;
    private @Nullable NtpThemeCollectionsCoordinator mNtpThemeCollectionsCoordinator;
    private @Nullable NtpChromeColorsCoordinator mNtpChromeColorsCoordinator;

    public NtpThemeMediator(
            Context context,
            PropertyModel bottomSheetPropertyModel,
            PropertyModel themePropertyModel,
            BottomSheetDelegate delegate,
            Profile profile,
            NtpCustomizationConfigManager ntpCustomizationConfigManager,
            @Nullable ActivityResultRegistry activityResultRegistry,
            Callback<@Nullable Bitmap> onImageSelectedCallback) {
        mContext = context;
        mProfile = profile;
        mBottomSheetPropertyModel = bottomSheetPropertyModel;
        mThemePropertyModel = themePropertyModel;
        mBottomSheetDelegate = delegate;
        mNtpCustomizationConfigManager = ntpCustomizationConfigManager;
        mActivityResultRegistry = activityResultRegistry;
        mOnImageSelectedCallback = onImageSelectedCallback;

        // Hides the back button when the theme settings bottom sheet is displayed standalone.
        mBottomSheetPropertyModel.set(
                BACK_PRESS_HANDLER,
                delegate.shouldShowAlone()
                        ? null
                        : v -> mBottomSheetDelegate.backPressOnCurrentBottomSheet());

        setOnClickListenerForAllSection();
        mThemePropertyModel.set(LEARN_MORE_BUTTON_CLICK_LISTENER, this::handleLearnMoreClick);
        initTrailingIcon();
        setLeadingIconForThemeCollectionsSection();
    }

    void destroy() {
        mCallbackController.destroy();
        mBottomSheetPropertyModel.set(BACK_PRESS_HANDLER, null);
        mThemePropertyModel.set(LEARN_MORE_BUTTON_CLICK_LISTENER, null);
        mActivityResultLauncher = null;
        mActivityResultRegistry = null;
        if (mNtpThemeCollectionsCoordinator != null) {
            mNtpThemeCollectionsCoordinator.destroy();
        }
        if (mNtpChromeColorsCoordinator != null) {
            mNtpChromeColorsCoordinator.destroy();
        }
    }

    /** Sets the on click listener for each theme bottom sheet section. */
    @VisibleForTesting
    void setOnClickListenerForAllSection() {
        if (mActivityResultRegistry != null) {
            mActivityResultLauncher =
                    mActivityResultRegistry.register(
                            UPLOAD_IMAGE_KEY,
                            new ActivityResultContracts.GetContent(),
                            uri -> {
                                // If users didn't select any file, the returned uri is null.
                                if (uri == null) return;

                                // When a new image is selected, store it and
                                // reset any existing crop settings from a previous
                                // image.
                                ShareImageFileUtils.getBitmapFromUriAsync(
                                        mContext, uri, mOnImageSelectedCallback);
                                updateTrailingIconVisibilityForSectionType(IMAGE_FROM_DISK);
                            });
        }

        mThemePropertyModel.set(
                SECTION_ON_CLICK_LISTENER,
                new Pair<>(DEFAULT, this::handleChromeDefaultSectionClick));
        mThemePropertyModel.set(
                SECTION_ON_CLICK_LISTENER,
                new Pair<>(IMAGE_FROM_DISK, this::handleUploadAnImageSectionClick));
        mThemePropertyModel.set(
                SECTION_ON_CLICK_LISTENER,
                new Pair<>(CHROME_COLOR, this::handleChromeColorsSectionClick));
        mThemePropertyModel.set(
                SECTION_ON_CLICK_LISTENER,
                new Pair<>(THEME_COLLECTION, this::handleThemeCollectionsSectionClick));
    }

    /**
     * Updates the visibility of the trailing icon for each theme section. The icon is made visible
     * for the section that matches {@code sectionType}, and hidden for all other sections.
     *
     * @param sectionType The {@link NtpBackgroundImageType} to show the trailing icon for.
     */
    private void updateTrailingIconVisibilityForSectionType(
            @NtpBackgroundImageType int sectionType) {
        for (int i = 0; i < NtpBackgroundImageType.NUM_ENTRIES; i++) {
            if (i == THEME_COLLECTION) {
                if (sectionType != THEME_COLLECTION && mNtpThemeCollectionsCoordinator != null) {
                    mNtpThemeCollectionsCoordinator.clearThemeCollectionSelection();
                }
                continue;
            }

            if (i == sectionType) {
                mThemePropertyModel.set(IS_SECTION_TRAILING_ICON_VISIBLE, new Pair<>(i, true));
            } else {
                mThemePropertyModel.set(IS_SECTION_TRAILING_ICON_VISIBLE, new Pair<>(i, false));
            }
        }
    }

    /**
     * Sets the primary image and the secondary image for the leading icon of the theme collections
     * section.
     */
    private void setLeadingIconForThemeCollectionsSection() {
        // TODO(crbug.com/423579377): Update the drawable.
        mThemePropertyModel.set(
                LEADING_ICON_FOR_THEME_COLLECTIONS,
                new Pair<>(
                        R.drawable.upload_an_image_icon_for_theme_bottom_sheet,
                        R.drawable.upload_an_image_icon_for_theme_bottom_sheet));
    }

    @VisibleForTesting
    void handleChromeDefaultSectionClick(View view) {
        updateTrailingIconVisibilityForSectionType(DEFAULT);

        @NtpBackgroundImageType
        int currentBackgroundType = mNtpCustomizationConfigManager.getBackgroundImageType();
        if (currentBackgroundType != DEFAULT) {
            // We need to update the app's theme when a customized background color is removed.
            mBottomSheetDelegate.onNewColorSelected(/* isDifferentColor= */ true);
        }
        mNtpCustomizationConfigManager.onBackgroundColorChanged(
                mContext, /* colorInfo= */ null, DEFAULT);
    }

    @VisibleForTesting
    void handleUploadAnImageSectionClick(View view) {
        if (mActivityResultLauncher != null) {
            mActivityResultLauncher.launch("image/*");
        }
    }

    @VisibleForTesting
    void handleChromeColorsSectionClick(View view) {
        if (mNtpChromeColorsCoordinator == null) {
            mNtpChromeColorsCoordinator =
                    new NtpChromeColorsCoordinator(
                            mContext,
                            mBottomSheetDelegate,
                            mCallbackController.makeCancelable(
                                    () -> {
                                        updateTrailingIconVisibilityForSectionType(CHROME_COLOR);
                                    }));
        }
        mBottomSheetDelegate.showBottomSheet(BottomSheetType.CHROME_COLORS);
    }

    @VisibleForTesting
    void handleThemeCollectionsSectionClick(View view) {
        if (mNtpThemeCollectionsCoordinator == null) {
            mNtpThemeCollectionsCoordinator =
                    new NtpThemeCollectionsCoordinator(
                            mContext,
                            mBottomSheetDelegate,
                            mProfile,
                            mCallbackController.makeCancelable(
                                    () -> {
                                        updateTrailingIconVisibilityForSectionType(
                                                THEME_COLLECTION);
                                    }));
        }
        mBottomSheetDelegate.showBottomSheet(BottomSheetType.THEME_COLLECTIONS);
    }

    @VisibleForTesting
    void handleLearnMoreClick(View view) {
        launchUriActivity(view.getContext(), LEARN_MORE_CLICK_URL);
    }

    /** Sets the initial visibility of the trailing icon based on the current theme settings. */
    private void initTrailingIcon() {
        @NtpBackgroundImageType
        int imageType = NtpCustomizationUtils.getNtpBackgroundImageTypeFromSharedPreference();
        updateTrailingIconVisibilityForSectionType(imageType);
    }

    void setNtpThemeCollectionsCoordinatorForTesting(
            NtpThemeCollectionsCoordinator ntpThemeCollectionsCoordinator) {
        mNtpThemeCollectionsCoordinator = ntpThemeCollectionsCoordinator;
    }

    public void setNtpChromeColorsCoordinatorForTesting(
            NtpChromeColorsCoordinator ntpChromeColorsCoordinator) {
        mNtpChromeColorsCoordinator = ntpChromeColorsCoordinator;
    }
}
