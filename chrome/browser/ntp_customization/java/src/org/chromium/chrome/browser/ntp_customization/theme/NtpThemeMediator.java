// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.launchUriActivity;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.CHROME_DEFAULT;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.UPLOAD_AN_IMAGE;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.IS_SECTION_TRAILING_ICON_VISIBLE;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LEADING_ICON_FOR_THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LEARN_MORE_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SECTION_ON_CLICK_LISTENER;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.util.Pair;
import android.view.View;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.ActivityResultRegistry;
import androidx.activity.result.contract.ActivityResultContracts;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection;
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
    private final PropertyModel mBottomSheetPropertyModel;
    private final PropertyModel mThemePropertyModel;
    private final BottomSheetDelegate mBottomSheetDelegate;
    private final Context mContext;
    private final NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    private @Nullable ActivityResultRegistry mActivityResultRegistry;
    private @Nullable ActivityResultLauncher<String> mActivityResultLauncher;
    private @Nullable NtpThemeCollectionsCoordinator mNtpThemeCollectionsCoordinator;

    public NtpThemeMediator(
            Context context,
            PropertyModel bottomSheetPropertyModel,
            PropertyModel themePropertyModel,
            BottomSheetDelegate delegate,
            Profile profile,
            NtpCustomizationConfigManager ntpCustomizationConfigManager,
            @Nullable ActivityResultRegistry activityResultRegistry) {
        mContext = context;
        mBottomSheetPropertyModel = bottomSheetPropertyModel;
        mThemePropertyModel = themePropertyModel;
        mBottomSheetDelegate = delegate;
        mNtpCustomizationConfigManager = ntpCustomizationConfigManager;
        mActivityResultRegistry = activityResultRegistry;

        // Hides the back button when the theme settings bottom sheet is displayed standalone.
        mBottomSheetPropertyModel.set(
                BACK_PRESS_HANDLER,
                delegate.shouldShowAlone()
                        ? null
                        : v -> mBottomSheetDelegate.backPressOnCurrentBottomSheet());

        setOnClickListenerForAllSection();
        mThemePropertyModel.set(LEARN_MORE_BUTTON_CLICK_LISTENER, this::handleLearnMoreClick);
        setLeadingIconForThemeCollectionsSection();
    }

    void destroy() {
        mBottomSheetPropertyModel.set(BACK_PRESS_HANDLER, null);
        mThemePropertyModel.set(LEARN_MORE_BUTTON_CLICK_LISTENER, null);
        mActivityResultLauncher = null;
        mActivityResultRegistry = null;
        if (mNtpThemeCollectionsCoordinator != null) {
            mNtpThemeCollectionsCoordinator.destroy();
        }
    }

    /** Sets the on click listener for each theme bottom sheet section. */
    private void setOnClickListenerForAllSection() {
        if (mActivityResultRegistry != null) {
            mActivityResultLauncher =
                    mActivityResultRegistry.register(
                            UPLOAD_IMAGE_KEY,
                            new ActivityResultContracts.GetContent(),
                            uri -> {
                                // If users didn't select any file, the returned uri is null.
                                if (uri == null) return;

                                ShareImageFileUtils.getBitmapFromUriAsync(
                                        mContext,
                                        uri,
                                        bitmap -> {
                                            mNtpCustomizationConfigManager.onBackgroundChanged(
                                                    bitmap);
                                        });
                            });
        }

        mThemePropertyModel.set(
                SECTION_ON_CLICK_LISTENER,
                new Pair<>(CHROME_DEFAULT, this::handleChromeDefaultSectionClick));
        mThemePropertyModel.set(
                SECTION_ON_CLICK_LISTENER,
                new Pair<>(UPLOAD_AN_IMAGE, this::handleUploadAnImageSectionClick));
        mThemePropertyModel.set(
                SECTION_ON_CLICK_LISTENER,
                new Pair<>(CHROME_COLORS, this::handleChromeColorsSectionClick));
        mThemePropertyModel.set(
                SECTION_ON_CLICK_LISTENER,
                new Pair<>(THEME_COLLECTIONS, this::handleThemeCollectionsSectionClick));
    }

    /**
     * Updates the visibility of the trailing icon for each theme section. The icon is made visible
     * for the section that matches {@code sectionType}, and hidden for all other sections.
     *
     * @param sectionType The {@link NTPThemeBottomSheetSection} to show the trailing icon for.
     */
    private void updateTrailingIconVisibilityForSectionType(
            @NTPThemeBottomSheetSection int sectionType) {
        for (int i = 0; i < NTPThemeBottomSheetSection.NUM_ENTRIES; i++) {
            if (i == THEME_COLLECTIONS) {
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
        updateTrailingIconVisibilityForSectionType(CHROME_DEFAULT);

        mNtpCustomizationConfigManager.onBackgroundChanged(/* bitmap= */ null);
    }

    @VisibleForTesting
    void handleUploadAnImageSectionClick(View view) {
        updateTrailingIconVisibilityForSectionType(UPLOAD_AN_IMAGE);

        if (mActivityResultLauncher != null) {
            mActivityResultLauncher.launch("image/*");
        }
    }

    @VisibleForTesting
    void handleChromeColorsSectionClick(View view) {
        updateTrailingIconVisibilityForSectionType(CHROME_COLORS);
    }

    @VisibleForTesting
    void handleThemeCollectionsSectionClick(View view) {
        updateTrailingIconVisibilityForSectionType(THEME_COLLECTIONS);

        if (mNtpThemeCollectionsCoordinator == null) {
            mNtpThemeCollectionsCoordinator =
                    new NtpThemeCollectionsCoordinator(mContext, mBottomSheetDelegate);
        }
        mBottomSheetDelegate.showBottomSheet(BottomSheetType.THEME_COLLECTIONS);
    }

    @VisibleForTesting
    void handleLearnMoreClick(View view) {
        launchUriActivity(view.getContext(), LEARN_MORE_CLICK_URL);
    }

    void setNtpThemeCollectionsCoordinatorForTesting(
            NtpThemeCollectionsCoordinator ntpThemeCollectionsCoordinator) {
        mNtpThemeCollectionsCoordinator = ntpThemeCollectionsCoordinator;
    }
}
