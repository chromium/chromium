// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.THEME_KEYS;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;

import androidx.activity.ComponentActivity;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.BottomSheetViewBinder;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the NTP appearance settings bottom sheet in the NTP customization. */
@NullMarked
public class NtpThemeCoordinator {
    private final Context mContext;
    private final BottomSheetDelegate mBottomSheetDelegate;
    private final Runnable mDismissBottomSheetRunnable;
    private NtpThemeMediator mMediator;
    private NtpThemeBottomSheetView mNtpThemeBottomSheetView;
    private @Nullable UploadImagePreviewCoordinator mUploadPreviewCoordinator;

    public NtpThemeCoordinator(
            Context context,
            BottomSheetDelegate delegate,
            Profile profile,
            Runnable dismissBottomSheet) {
        mContext = context;
        mBottomSheetDelegate = delegate;
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

        mMediator =
                new NtpThemeMediator(
                        context,
                        bottomSheetPropertyModel,
                        themePropertyModel,
                        delegate,
                        profile,
                        NtpCustomizationConfigManager.getInstance(),
                        activityResultRegistry,
                        this::onImageSelectedForPreview);
    }

    /**
     * This is the callback method that gets invoked by the Mediator to initialize the {@code
     * UploadImagePreviewCoordinator}.
     */
    public void onImageSelectedForPreview(@Nullable Bitmap bitmap) {
        if (bitmap == null) return;

        mUploadPreviewCoordinator =
                new UploadImagePreviewCoordinator(
                        (Activity) mContext, bitmap, this::onPreviewClosed);
    }

    public void destroy() {
        mMediator.destroy();
        mNtpThemeBottomSheetView.destroy();
        if (mUploadPreviewCoordinator != null) {
            mUploadPreviewCoordinator.destroy();
        }
    }

    @VisibleForTesting
    void onPreviewClosed(boolean isImageSelected) {
        if (isImageSelected) {
            mBottomSheetDelegate.onNewColorSelected(/* isDifferentColor */ true);
        }
        mDismissBottomSheetRunnable.run();
    }

    NtpThemeMediator getMediatorForTesting() {
        return mMediator;
    }

    void setMediatorForTesting(NtpThemeMediator mediator) {
        mMediator = mediator;
    }

    void setNtpThemeBottomSheetViewForTesting(NtpThemeBottomSheetView ntpThemeBottomSheetView) {
        mNtpThemeBottomSheetView = ntpThemeBottomSheetView;
    }
}
