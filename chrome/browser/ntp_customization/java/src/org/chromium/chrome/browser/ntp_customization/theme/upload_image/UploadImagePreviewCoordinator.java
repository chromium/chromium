// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.doesDefaultSearchEngineHaveLogo;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LOGO_BITMAP;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LOGO_PARAMS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LOGO_VISIBILITY;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_KEYS;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Point;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationMetricsUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Coordinator for managing the Upload Image Preview dialog. */
@NullMarked
public class UploadImagePreviewCoordinator implements InsetObserver.WindowInsetsConsumer {

    private final PropertyModel mPreviewPropertyModel;
    private final ChromeDialog mDialog;
    private final int mToolBarHeight;
    private CropImageView mCropImageView;

    /**
     * The type of user interactions with the Upload Image Preview dialog.
     *
     * <p>These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused. See tools/metrics/histograms/enums.xml.
     */
    @IntDef({
        PreviewInteractionType.CANCEL,
        PreviewInteractionType.SAVE,
        PreviewInteractionType.PINCH_TO_RESIZE,
        PreviewInteractionType.ROTATE_SCREEN,
        PreviewInteractionType.ROTATE_SCREEN_AND_PINCH_TO_RESIZE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PreviewInteractionType {
        int CANCEL = 0;
        int SAVE = 1;
        int PINCH_TO_RESIZE = 2; // Scale and scroll
        int ROTATE_SCREEN = 3;
        int ROTATE_SCREEN_AND_PINCH_TO_RESIZE = 4;
        int NUM_ENTRIES = 5;
    }

    /**
     * @param activity The activity context.
     * @param bitmap The bitmap to be previewed.
     */
    public UploadImagePreviewCoordinator(
            Activity activity,
            Profile profile,
            Bitmap bitmap,
            Callback<Boolean> onBottomSheetClickedCallback) {
        mPreviewPropertyModel = new PropertyModel(PREVIEW_KEYS);
        UploadImagePreviewLayout previewLayout =
                (UploadImagePreviewLayout)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.ntp_customization_theme_preview_dialog_layout,
                                        null);
        mCropImageView = previewLayout.findViewById(R.id.preview_image);
        mToolBarHeight =
                activity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);

        mDialog =
                new ChromeDialog(
                        activity,
                        /* themeResId= */ R.style.ThemeOverlay_BrowserUI_Fullscreen,
                        /* shouldPadForWindowInsets= */ false);
        mDialog.addInsetsConsumer(this, InsetConsumerSource.UPLOAD_IMAGE_PREVIEW_DIALOG);
        mDialog.setContentView(previewLayout);

        PropertyModelChangeProcessor.create(
                mPreviewPropertyModel, previewLayout, UploadImagePreviewLayoutViewBinder::bind);

        mPreviewPropertyModel.set(NtpThemeProperty.BITMAP_FOR_PREVIEW, bitmap);

        mPreviewPropertyModel.set(
                NtpThemeProperty.PREVIEW_SAVE_CLICK_LISTENER,
                v -> {
                    onSaveButtonClicked(bitmap, onBottomSheetClickedCallback, mDialog);
                    NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewInteractions(
                            PreviewInteractionType.SAVE);
                    recordPreviewInteractionsMetric();
                });

        mPreviewPropertyModel.set(
                NtpThemeProperty.PREVIEW_CANCEL_CLICK_LISTENER,
                v -> {
                    onBottomSheetClickedCallback.onResult(false);
                    mDialog.dismiss();
                    NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewInteractions(
                            PreviewInteractionType.CANCEL);
                    recordPreviewInteractionsMetric();
                });

        if (ChromeFeatureList.sNewTabPageCustomizationV2ShowLogoAndSearchBox.getValue()) {
            setUpLogo(activity, profile, mPreviewPropertyModel);
        }

        mDialog.show();
        NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewShow();
    }

    @Override
    public WindowInsetsCompat onApplyWindowInsets(
            View view, WindowInsetsCompat windowInsetsCompat) {
        int statusBarHeight =
                windowInsetsCompat.getInsets(WindowInsetsCompat.Type.systemBars()).top;
        int navigationBarHeight =
                windowInsetsCompat.getInsets(WindowInsetsCompat.Type.navigationBars()).bottom;

        mPreviewPropertyModel.set(NtpThemeProperty.TOP_INSETS, mToolBarHeight + statusBarHeight);
        mPreviewPropertyModel.set(NtpThemeProperty.BOTTOM_MARGIN, navigationBarHeight);

        // Consumes the insets since the root view already adjusted their paddings.
        return new WindowInsetsCompat.Builder(windowInsetsCompat)
                .setInsets(WindowInsetsCompat.Type.statusBars(), Insets.NONE)
                .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.NONE)
                .build();
    }

    /**
     * Records the user interaction metrics for the image preview. When the user rotates the screen
     * and pinches to resize together (the order doesn't matter), only {@link
     * PreviewInteractionType#ROTATE_SCREEN_AND_PINCH_TO_RESIZE} is recorded once. Individual {@link
     * PreviewInteractionType#ROTATE_SCREEN} and {@link PreviewInteractionType#PINCH_TO_RESIZE}
     * metrics are not recorded to avoid double counting.
     */
    private void recordPreviewInteractionsMetric() {
        boolean isScaled = mCropImageView.getIsScaled();
        boolean isScrolled = mCropImageView.getIsScrolled();
        boolean isScreenRotated = mCropImageView.getIsScreenRotated();

        if ((isScaled || isScrolled) && isScreenRotated) {
            NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewInteractions(
                    PreviewInteractionType.ROTATE_SCREEN_AND_PINCH_TO_RESIZE);
            return;
        }

        if (isScaled || isScrolled) {
            NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewInteractions(
                    PreviewInteractionType.PINCH_TO_RESIZE);
            return;
        }

        if (isScreenRotated) {
            NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewInteractions(
                    PreviewInteractionType.ROTATE_SCREEN);
        }
    }

    /**
     * Configures the search engine logo's visibility and content.
     *
     * <p>This method handles four distinct logo states:
     *
     * <ul>
     *   <li><b>No Logo:</b> If {@code shouldShowLogo} is false, the view is hidden.
     *   <li><b>Third-Party Loading:</b> If a third-party engine is selected but the bitmap is
     *       {@code null} (e.g., currently fetching, offline, or unavailable), the view is hidden.
     *   <li><b>Google Logo:</b> If Google is the DSE and {@code logoBitmap} is {@code null} (e.g.,
     *       standard logo or Doodle is still loading), the default Google drawable is used.
     *   <li><b>Doodle / Third-Party Logo:</b> If a valid bitmap is provided, it is displayed and
     *       the layout parameters are dynamically adjusted.
     * </ul>
     *
     * @param activity The current activity, used for resource retrieval and multi-window mode
     *     checks.
     * @param profile The user profile, used to determine the default search engine status.
     * @param model The {@link PropertyModel} to update with the calculated logo state.
     */
    private void setUpLogo(Activity activity, Profile profile, PropertyModel model) {
        boolean shouldShowLogo = doesDefaultSearchEngineHaveLogo(profile);
        boolean isGoogleDSE =
                TemplateUrlServiceFactory.getForProfile(profile).isDefaultSearchEngineGoogle();
        Bitmap logoBitmap =
                NtpCustomizationConfigManager.getInstance().getDefaultSearchEngineLogoBitmap();

        if (!shouldShowLogo || (!isGoogleDSE && logoBitmap == null)) {
            model.set(LOGO_VISIBILITY, View.GONE);
            return;
        }

        model.set(LOGO_VISIBILITY, View.VISIBLE);
        model.set(LOGO_BITMAP, logoBitmap);
        model.set(
                LOGO_PARAMS,
                LogoUtils.getLogoViewLayoutParams(
                        activity.getResources(),
                        /* isLogoDoodle= */ logoBitmap != null,
                        LogoUtils.getDoodleSize(activity.isInMultiWindowMode())));
    }

    PropertyModel getPropertyModelForTesting() {
        return mPreviewPropertyModel;
    }

    public void destroy() {
        mPreviewPropertyModel.set(NtpThemeProperty.PREVIEW_SAVE_CLICK_LISTENER, null);
        mPreviewPropertyModel.set(NtpThemeProperty.PREVIEW_CANCEL_CLICK_LISTENER, null);
        NtpCustomizationConfigManager.getInstance().setDefaultSearchEngineLogoBitmap(null);
        mDialog.destroy();
    }

    /**
     * Called when the save button is clicked.
     *
     * @param bitmap The selected bitmap.
     * @param onBottomSheetClickedCallback The callback to be notified when a bottom sheet button is
     *     clicked.
     * @param dialog The current preview dialog.
     */
    @VisibleForTesting
    void onSaveButtonClicked(
            Bitmap bitmap, Callback<Boolean> onBottomSheetClickedCallback, ChromeDialog dialog) {
        // 1. Gets the matrices (source of truth or calculated estimate)
        Matrix portraitMatrix = mCropImageView.getPortraitMatrix();
        Matrix landscapeMatrix = mCropImageView.getLandscapeMatrix();

        // 2. Gets the dimensions used to create those matrices
        // Note: These might be "guessed" dimensions if the user didn't rotate.
        Point portraitSize = mCropImageView.getPortraitWindowSize();
        Point landscapeSize = mCropImageView.getLandscapeWindowSize();

        BackgroundImageInfo info =
                new BackgroundImageInfo(
                        portraitMatrix, landscapeMatrix, portraitSize, landscapeSize);

        NtpCustomizationConfigManager.getInstance().onUploadedImageSelected(bitmap, info);

        onBottomSheetClickedCallback.onResult(true);
        dialog.dismiss();
    }

    void setCropImageViewForTesting(CropImageView view) {
        mCropImageView = view;
    }
}
