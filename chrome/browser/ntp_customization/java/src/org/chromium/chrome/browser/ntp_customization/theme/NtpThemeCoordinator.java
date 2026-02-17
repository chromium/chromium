// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.THEME_KEYS;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;

import androidx.activity.ComponentActivity;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.BottomSheetViewBinder;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpChromeColorsCoordinator;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.BackgroundCollection;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionManager;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsCoordinator;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.UploadImagePreviewCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/** Coordinator for the NTP appearance settings bottom sheet in the NTP customization. */
@NullMarked
public class NtpThemeCoordinator {
    private final Context mContext;
    private final BottomSheetDelegate mBottomSheetDelegate;
    private final Profile mProfile;
    private final NtpThemeDelegate mNtpThemeDelegate;
    private final Runnable mDismissBottomSheetRunnable;
    private final NtpThemeCollectionManager mNtpThemeCollectionManager;
    private final CallbackController mCallbackController = new CallbackController();
    private NtpThemeMediator mMediator;
    private NtpThemeBottomSheetView mNtpThemeBottomSheetView;
    private @Nullable UploadImagePreviewCoordinator mUploadPreviewCoordinator;
    private @Nullable NtpThemeCollectionsCoordinator mNtpThemeCollectionsCoordinator;
    private @Nullable NtpChromeColorsCoordinator mNtpChromeColorsCoordinator;

    public NtpThemeCoordinator(
            Context context,
            BottomSheetDelegate delegate,
            Profile profile,
            Runnable dismissBottomSheet) {
        mContext = context;
        mBottomSheetDelegate = delegate;
        mProfile = profile;
        mDismissBottomSheetRunnable = dismissBottomSheet;
        mNtpThemeBottomSheetView =
                (NtpThemeBottomSheetView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.ntp_customization_theme_bottom_sheet_layout,
                                        null,
                                        false);

        delegate.registerBottomSheetLayout(THEME, mNtpThemeBottomSheetView);

        // The bottomSheetPropertyModel is responsible for managing the back press handler of the
        // back button in the bottom sheet.
        PropertyModel bottomSheetPropertyModel =
                new PropertyModel(NtpCustomizationViewProperties.BOTTOM_SHEET_KEYS);
        PropertyModelChangeProcessor.create(
                bottomSheetPropertyModel, mNtpThemeBottomSheetView, BottomSheetViewBinder::bind);

        // The themePropertyModel is responsible for managing the learn more button in the theme
        // bottom sheet.
        PropertyModel themePropertyModel = new PropertyModel(THEME_KEYS);
        PropertyModelChangeProcessor.create(
                themePropertyModel,
                mNtpThemeBottomSheetView,
                NtpThemeViewBinder::bindThemeBottomSheet);

        var activityResultRegistry =
                context instanceof ComponentActivity
                        ? ((ComponentActivity) context).getActivityResultRegistry()
                        : null;

        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(
                        mContext,
                        profile,
                        mCallbackController.makeCancelable(
                                (Bitmap bitmap) -> {
                                    initializeBottomSheetContent(
                                            BottomSheetType.SINGLE_THEME_COLLECTION);
                                    initializeBottomSheetContent(BottomSheetType.THEME_COLLECTIONS);
                                    mMediator.updateTrailingIconVisibilityForSectionType(
                                            THEME_COLLECTION);
                                    mBottomSheetDelegate.onNewColorSelected(
                                            /* isDifferentColor= */ true);
                                    mBottomSheetDelegate.onNewThemeCollectionImageSelected(bitmap);
                                }));
        mNtpThemeDelegate = createNtpThemeDelegate();
        mMediator =
                new NtpThemeMediator(
                        context,
                        mProfile,
                        bottomSheetPropertyModel,
                        themePropertyModel,
                        delegate,
                        NtpCustomizationConfigManager.getInstance(),
                        activityResultRegistry,
                        this::onImageSelectedForPreview,
                        mNtpThemeDelegate,
                        mNtpThemeCollectionManager);
    }

    /**
     * This is the callback method that gets invoked by the Mediator to initialize the {@code
     * UploadImagePreviewCoordinator}.
     */
    public void onImageSelectedForPreview(@Nullable Bitmap bitmap) {
        if (bitmap == null) return;

        // Tablets bypass the preview dialog and apply the selection directly.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            // Applies the background immediately for instant visual feedback.
            // Full Activity recreation to finalize theme changes is deferred
            // until the ntp customization bottom sheets are fully dismissed.
            BackgroundImageInfo info =
                    NtpCustomizationUtils.getDefaultBackgroundImageInfo(mContext, bitmap);
            NtpCustomizationConfigManager.getInstance().onUploadedImageSelected(bitmap, info);
            onPreviewClosed(/* isImageSelected= */ true);
            return;
        }

        mUploadPreviewCoordinator =
                new UploadImagePreviewCoordinator(
                        (Activity) mContext, mProfile, bitmap, this::onPreviewClosed);
    }

    /**
     * Creates and returns a {@link NtpThemeDelegate} that handles interactions with the theme
     * customization options. This delegate is responsible for creating and showing the appropriate
     * sub-coordinators when the user selects different theme options like "Chrome Colors" or "Theme
     * Collections."
     *
     * @return A new {@link NtpThemeDelegate} instance.
     */
    private NtpThemeDelegate createNtpThemeDelegate() {
        return new NtpThemeDelegate() {
            @Override
            public void onChromeColorsClicked() {
                if (mNtpChromeColorsCoordinator == null) {
                    Runnable onChromeColorSelectedCallback =
                            () -> {
                                mMediator.updateForChoosingDefaultOrChromeColorOption(CHROME_COLOR);
                            };

                    mNtpChromeColorsCoordinator =
                            new NtpChromeColorsCoordinator(
                                    mContext,
                                    mBottomSheetDelegate,
                                    mCallbackController.makeCancelable(
                                            onChromeColorSelectedCallback));
                }
                mNtpChromeColorsCoordinator.prepareToShow();
                mBottomSheetDelegate.showBottomSheet(CHROME_COLORS);
            }

            @Override
            public void onThemeCollectionsClicked(
                    Runnable resetCustomizedThemeRunnable,
                    List<BackgroundCollection> themeCollectionsList) {
                if (mNtpThemeCollectionsCoordinator == null) {
                    Runnable onDailyRefreshCancelledCallback =
                            () -> {
                                resetCustomizedThemeRunnable.run();
                                initializeBottomSheetContent(
                                        BottomSheetType.SINGLE_THEME_COLLECTION);
                            };
                    mNtpThemeCollectionsCoordinator =
                            new NtpThemeCollectionsCoordinator(
                                    mContext,
                                    mBottomSheetDelegate,
                                    mProfile,
                                    mNtpThemeCollectionManager,
                                    onDailyRefreshCancelledCallback,
                                    themeCollectionsList);
                }
                mBottomSheetDelegate.showBottomSheet(THEME_COLLECTIONS);
            }
        };
    }

    public void destroy() {
        mMediator.destroy();
        mNtpThemeBottomSheetView.destroy();
        if (mUploadPreviewCoordinator != null) {
            mUploadPreviewCoordinator.destroy();
            mUploadPreviewCoordinator = null;
        }
        if (mNtpThemeCollectionsCoordinator != null) {
            mNtpThemeCollectionsCoordinator.destroy();
            mNtpThemeCollectionsCoordinator = null;
        }
        if (mNtpChromeColorsCoordinator != null) {
            mNtpChromeColorsCoordinator.destroy();
            mNtpChromeColorsCoordinator = null;
        }
        mNtpThemeCollectionManager.destroy();
        mNtpThemeCollectionsCoordinator = null;

        mCallbackController.destroy();
    }

    /**
     * Initialize the bottom sheet content of the given bottom sheet type when it becomes visible.
     *
     * @param bottomSheetType The type of the bottom sheet to update.
     */
    public void initializeBottomSheetContent(@BottomSheetType int bottomSheetType) {
        if (mNtpThemeCollectionsCoordinator != null) {
            mNtpThemeCollectionsCoordinator.initializeBottomSheetContent(bottomSheetType);
        }
    }

    @VisibleForTesting
    void onPreviewClosed(boolean isImageSelected) {
        if (!isImageSelected) {
            return;
        }
        onImageSelectedForPreviewImpl();
        mDismissBottomSheetRunnable.run();
    }

    /**
     * Finalizes the selection of a device-stored image by updating theme-related information.
     *
     * <ul>
     *   <li>Updates the Mediator to show the selection indicator for the "Upload an image" section.
     *   <li>Triggers the delegate callback to handle the theme changes.
     * </ul>
     */
    private void onImageSelectedForPreviewImpl() {
        mMediator.updateTrailingIconVisibilityForSectionType(IMAGE_FROM_DISK);
        mBottomSheetDelegate.onNewColorSelected(/* isDifferentColor= */ true);
    }

    NtpThemeMediator getMediatorForTesting() {
        return mMediator;
    }

    void setMediatorForTesting(NtpThemeMediator mediator) {
        var oldValue = mMediator;
        mMediator = mediator;
        ResettersForTesting.register(() -> mMediator = oldValue);
    }

    void setNtpThemeBottomSheetViewForTesting(NtpThemeBottomSheetView ntpThemeBottomSheetView) {
        mNtpThemeBottomSheetView = ntpThemeBottomSheetView;
    }

    NtpThemeCollectionManager getNtpThemeManagerForTesting() {
        return mNtpThemeCollectionManager;
    }

    NtpThemeDelegate getNtpThemeDelegateForTesting() {
        return mNtpThemeDelegate;
    }

    void setNtpThemeCollectionsCoordinatorForTesting(
            NtpThemeCollectionsCoordinator ntpThemeCollectionsCoordinator) {
        mNtpThemeCollectionsCoordinator = ntpThemeCollectionsCoordinator;
    }
}
