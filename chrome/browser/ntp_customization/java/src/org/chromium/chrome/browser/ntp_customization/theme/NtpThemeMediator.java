// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.COLOR_FROM_HEX;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.DEFAULT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.launchUriActivity;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.IS_SECTION_SELECTED;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LEADING_ICON_FOR_THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LEARN_MORE_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SECTION_ON_CLICK_LISTENER;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.util.Pair;
import android.view.View;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.ActivityResultRegistry;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationMetricsUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.BackgroundCollection;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

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
    private final Callback<@Nullable Bitmap> mOnImageSelectedCallback;
    private final NtpThemeDelegate mNtpThemeDelegate;
    private final NtpThemeCollectionManager mNtpThemeCollectionManager;
    private final Profile mProfile;
    private final List<BackgroundCollection> mThemeCollectionsList = new ArrayList<>();
    private @Nullable ActivityResultRegistry mActivityResultRegistry;
    private @Nullable ActivityResultLauncher<String> mActivityResultLauncher;

    public NtpThemeMediator(
            Context context,
            Profile profile,
            PropertyModel bottomSheetPropertyModel,
            PropertyModel themePropertyModel,
            BottomSheetDelegate delegate,
            NtpCustomizationConfigManager ntpCustomizationConfigManager,
            @Nullable ActivityResultRegistry activityResultRegistry,
            Callback<@Nullable Bitmap> onImageSelectedCallback,
            NtpThemeDelegate ntpThemeDelegate,
            NtpThemeCollectionManager ntpThemeCollectionManager) {
        mContext = context;
        mProfile = profile;
        mBottomSheetPropertyModel = bottomSheetPropertyModel;
        mThemePropertyModel = themePropertyModel;
        mBottomSheetDelegate = delegate;
        mNtpCustomizationConfigManager = ntpCustomizationConfigManager;
        mActivityResultRegistry = activityResultRegistry;
        mOnImageSelectedCallback = onImageSelectedCallback;
        mNtpThemeDelegate = ntpThemeDelegate;
        mNtpThemeCollectionManager = ntpThemeCollectionManager;

        // Hides the back button when the theme settings bottom sheet is displayed standalone.
        mBottomSheetPropertyModel.set(
                BACK_PRESS_HANDLER,
                delegate.shouldShowAlone()
                        ? null
                        : v -> mBottomSheetDelegate.backPressOnCurrentBottomSheet());

        setOnClickListenerForAllSection();
        mThemePropertyModel.set(LEARN_MORE_BUTTON_CLICK_LISTENER, this::handleLearnMoreClick);
        initTrailingIcon();
        fetchAndSetThemeCollectionsLeadingIcon();
    }

    void destroy() {
        mBottomSheetPropertyModel.set(BACK_PRESS_HANDLER, null);
        mThemePropertyModel.set(LEARN_MORE_BUTTON_CLICK_LISTENER, null);
        mThemePropertyModel.set(SECTION_ON_CLICK_LISTENER, new Pair<>(DEFAULT, null));
        mThemePropertyModel.set(SECTION_ON_CLICK_LISTENER, new Pair<>(IMAGE_FROM_DISK, null));
        mThemePropertyModel.set(SECTION_ON_CLICK_LISTENER, new Pair<>(CHROME_COLOR, null));
        mThemePropertyModel.set(SECTION_ON_CLICK_LISTENER, new Pair<>(THEME_COLLECTION, null));
        if (mActivityResultLauncher != null) {
            mActivityResultLauncher.unregister();
            mActivityResultLauncher = null;
        }
        mActivityResultRegistry = null;
    }

    /** Sets the on click listener for each theme bottom sheet section. */
    @VisibleForTesting
    void setOnClickListenerForAllSection() {
        if (mActivityResultRegistry != null) {
            mActivityResultLauncher =
                    mActivityResultRegistry.register(
                            UPLOAD_IMAGE_KEY,
                            new ActivityResultContracts.GetContent(),
                            this::onUploadImageResult);
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
     * @param sectionType The {@link NtpBackgroundType} to show the trailing icon for.
     */
    @VisibleForTesting
    void updateTrailingIconVisibilityForSectionType(@NtpBackgroundType int sectionType) {
        for (int i = 0; i < NtpBackgroundType.NUM_ENTRIES; i++) {
            if (i == COLOR_FROM_HEX && sectionType == CHROME_COLOR) {
                // Prevents overriding the visibility from visible to invisible if the user chooses
                // a customized color theme. This is because both types share the same bottom sheet
                // list item.
                continue;
            }

            if (i == sectionType) {
                mThemePropertyModel.set(IS_SECTION_SELECTED, new Pair<>(i, true));
            } else {
                mThemePropertyModel.set(IS_SECTION_SELECTED, new Pair<>(i, false));
            }
        }
    }

    /**
     * Handles the result of the activity launched to upload an image. If a URI is provided, it
     * attempts to decode the image and updates the UI.
     *
     * @param uri The URI of the selected image, or null if no image was selected.
     */
    @VisibleForTesting
    void onUploadImageResult(Uri uri) {
        // If users didn't select any file, the returned uri is null.
        if (uri != null) {
            // When a new image is selected, store it and reset any existing crop settings from a
            // previous image.
            NtpCustomizationUtils.getBitmapFromUriAsync(mContext, uri, mOnImageSelectedCallback);
            mNtpThemeCollectionManager.selectLocalBackgroundImage();
        }

        NtpCustomizationMetricsUtils.recordBottomSheetShown(BottomSheetType.UPLOAD_IMAGE);
    }

    @VisibleForTesting
    void handleChromeDefaultSectionClick(View view) {
        resetCustomizedTheme();

        NtpCustomizationMetricsUtils.recordBottomSheetShown(BottomSheetType.CHROME_DEFAULT);
    }

    /**
     * Handles clicks on the 'Chrome default' theme section or when the daily update feature is
     * cancelled.
     */
    private void resetCustomizedTheme() {
        updateForChoosingDefaultOrChromeColorOption(DEFAULT);

        @NtpBackgroundType
        int currentBackgroundType = mNtpCustomizationConfigManager.getBackgroundType();
        if (currentBackgroundType != DEFAULT) {
            // We need to update the app's theme when a customized background color is removed.
            mBottomSheetDelegate.onNewColorSelected(/* isDifferentColor= */ true);
        }
        mNtpCustomizationConfigManager.onBackgroundReset();
    }

    @VisibleForTesting
    void handleUploadAnImageSectionClick(View view) {
        if (mActivityResultLauncher != null) {
            mActivityResultLauncher.launch("image/*");
        }
    }

    @VisibleForTesting
    void handleChromeColorsSectionClick(View view) {
        mNtpThemeDelegate.onChromeColorsClicked();
    }

    @VisibleForTesting
    void handleThemeCollectionsSectionClick(View view) {
        mNtpThemeDelegate.onThemeCollectionsClicked(
                this::resetCustomizedTheme, mThemeCollectionsList);
    }

    @VisibleForTesting
    void handleLearnMoreClick(View view) {
        launchUriActivity(view.getContext(), LEARN_MORE_CLICK_URL);
    }

    /** Sets the initial visibility of the trailing icon based on the current theme settings. */
    private void initTrailingIcon() {
        @NtpBackgroundType
        int imageType = NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference();
        updateTrailingIconVisibilityForSectionType(imageType);
    }

    /**
     * Reset custom background info and update trailing icon visibility when the user selects the
     * default background or a Chrome color.
     */
    @VisibleForTesting
    void updateForChoosingDefaultOrChromeColorOption(@NtpBackgroundType int sectionType) {
        updateTrailingIconVisibilityForSectionType(sectionType);
        mNtpThemeCollectionManager.resetCustomBackground();
    }

    /**
     * Fetches theme collections and sets the leading icon for the theme collections section with
     * cover images from two selected collections. This method fetches two preview images in
     * parallel and updates the UI once both image fetches are complete.
     */
    @VisibleForTesting
    void fetchAndSetThemeCollectionsLeadingIcon() {
        mNtpThemeCollectionManager.getBackgroundCollections(
                (collections) -> {
                    mThemeCollectionsList.clear();
                    if (collections != null) {
                        mThemeCollectionsList.addAll(collections);
                    }

                    // TODO(crbug.com/423579377): Decide if these indices should be dynamic.
                    int firstIndex = 0;
                    GURL firstImageUrl =
                            mThemeCollectionsList.size() > firstIndex
                                    ? mThemeCollectionsList.get(firstIndex).previewImageUrl
                                    : null;

                    int secondIndex = 1;
                    GURL secondImageUrl =
                            mThemeCollectionsList.size() > secondIndex
                                    ? mThemeCollectionsList.get(secondIndex).previewImageUrl
                                    : null;

                    ImageFetcher imageFetcher = NtpCustomizationUtils.createImageFetcher(mProfile);
                    // Array to store the fetched bitmaps. Index 0 for the first image, Index 1 for
                    // the second.
                    final @Nullable Bitmap[] bitmaps = new Bitmap[2];
                    // Counter to track how many image fetches have completed.
                    // Using AtomicInteger for thread safety, ensuring atomic increments.
                    final AtomicInteger finishedCount = new AtomicInteger(0);
                    final int totalCount = 2;

                    // This Runnable is called after each image fetch attempt (success or failure).
                    // When both fetches are complete, it updates the leading icon.
                    Runnable onBothImagesFetched =
                            () -> {
                                // Atomically increment the count and check if all fetches are done.
                                if (finishedCount.incrementAndGet() == totalCount) {
                                    setLeadingIconFromBitmaps(bitmaps[0], bitmaps[1]);
                                }
                            };

                    // Create callbacks for handling the result of each image fetch.
                    Callback<@Nullable Bitmap> firstImageCallback =
                            createBitmapCallback(bitmaps, /* index= */ 0, onBothImagesFetched);
                    Callback<@Nullable Bitmap> secondImageCallback =
                            createBitmapCallback(bitmaps, /* index= */ 1, onBothImagesFetched);

                    // Fetch the images. If the URL is null, the callback is invoked immediately
                    // with null.
                    fetchImageOrRunCallback(imageFetcher, firstImageUrl, firstImageCallback);
                    fetchImageOrRunCallback(imageFetcher, secondImageUrl, secondImageCallback);
                });
    }

    /**
     * Creates a Callback for handling a fetched Bitmap. The callback stores the received Bitmap in
     * the specified index of the {@code bitmaps} array and then executes the {@code
     * onBothImagesFetched} Runnable.
     *
     * @param bitmaps The array where the fetched Bitmap will be stored.
     * @param index The index in the {@code bitmaps} array to store the result.
     * @param onBothImagesFetched The Runnable to execute after the bitmap is stored.
     */
    @VisibleForTesting
    Callback<@Nullable Bitmap> createBitmapCallback(
            final @Nullable Bitmap[] bitmaps, final int index, final Runnable onBothImagesFetched) {
        return (bitmap) -> {
            if (index >= 0 && index < bitmaps.length) {
                bitmaps[index] = bitmap;
            }
            onBothImagesFetched.run();
        };
    }

    /**
     * Helper method to either fetch an image using the ImageFetcher if the URL is not null, or to
     * immediately invoke the callback with a null Bitmap if the URL is null.
     *
     * @param imageFetcher The ImageFetcher instance to use for fetching.
     * @param imageUrl The URL of the image to fetch. Can be null.
     * @param callback The Callback to be invoked with the result (Bitmap or null).
     */
    @VisibleForTesting
    void fetchImageOrRunCallback(
            @Nullable ImageFetcher imageFetcher,
            @Nullable GURL imageUrl,
            Callback<@Nullable Bitmap> callback) {
        if (imageFetcher != null && imageUrl != null) {
            NtpCustomizationUtils.fetchThemeCollectionImage(imageFetcher, imageUrl, callback);
        } else {
            // If URL is null, report back with a null bitmap immediately.
            callback.onResult(null);
        }
    }

    /** Sets the leading icon for the theme collections section from two bitmaps. */
    @VisibleForTesting
    void setLeadingIconFromBitmaps(@Nullable Bitmap firstBitmap, @Nullable Bitmap secondBitmap) {
        Resources res = mContext.getResources();
        Drawable firstDrawable =
                (firstBitmap != null) ? new BitmapDrawable(res, firstBitmap) : null;
        Drawable secondDrawable =
                (secondBitmap != null) ? new BitmapDrawable(res, secondBitmap) : null;
        mThemePropertyModel.set(
                LEADING_ICON_FOR_THEME_COLLECTIONS, new Pair<>(firstDrawable, secondDrawable));
    }
}
