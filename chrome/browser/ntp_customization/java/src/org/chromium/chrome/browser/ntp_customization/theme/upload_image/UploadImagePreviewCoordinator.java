// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.doesDefaultSearchEngineHaveLogo;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.getSearchBoxTwoSideMargin;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.BUTTON_BOTTOM_MARGIN;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LOGO_BITMAP;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LOGO_PARAMS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LOGO_VISIBILITY;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.PREVIEW_KEYS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SEARCH_BOX_HEIGHT;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SEARCH_BOX_TOP_MARGIN;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.feed.FeedStreamViewResizerUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationMetricsUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
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
    private final boolean mShouldShowLogoAndSearchBox;
    private final Activity mActivity;
    private final UiConfig mUiConfig;
    private final int mButtonBottomMargin;
    private View.@Nullable OnLayoutChangeListener mLayoutChangeListener;
    private @Nullable UploadImagePreviewLayout mPreviewLayout;
    private @Nullable CropImageView mCropImageView;

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
        mActivity = activity;
        mPreviewLayout =
                (UploadImagePreviewLayout)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.ntp_customization_theme_preview_dialog_layout,
                                        null);
        mCropImageView = mPreviewLayout.findViewById(R.id.preview_image);
        mToolBarHeight =
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        mButtonBottomMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.ntp_customization_back_button_margin_start);

        mUiConfig = new UiConfig(mPreviewLayout);
        mShouldShowLogoAndSearchBox =
                ChromeFeatureList.sNewTabPageCustomizationV2ShowLogoAndSearchBox.getValue();
        mLayoutChangeListener =
                (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    // Checks if the bounding box has actually changed to avoid redundant calls.
                    if (left == oldLeft
                            && top == oldTop
                            && right == oldRight
                            && bottom == oldBottom) {
                        return;
                    }

                    mUiConfig.updateDisplayStyle();
                    if (mShouldShowLogoAndSearchBox) {
                        updateSearchBoxWidthPreview();
                    }
                };
        mPreviewLayout.addOnLayoutChangeListener(mLayoutChangeListener);

        mDialog =
                new ChromeDialog(
                        activity,
                        /* themeResId= */ R.style.ThemeOverlay_BrowserUI_Fullscreen,
                        /* shouldPadForWindowInsets= */ false);
        mDialog.addInsetsConsumer(this, InsetConsumerSource.UPLOAD_IMAGE_PREVIEW_DIALOG);
        mDialog.setContentView(mPreviewLayout);

        PropertyModelChangeProcessor.create(
                mPreviewPropertyModel, mPreviewLayout, UploadImagePreviewLayoutViewBinder::bind);

        mPreviewPropertyModel.set(NtpThemeProperty.BITMAP_FOR_PREVIEW, bitmap);

        mPreviewPropertyModel.set(
                NtpThemeProperty.PREVIEW_SAVE_CLICK_LISTENER,
                v -> {
                    onSaveButtonClicked(bitmap, onBottomSheetClickedCallback, mDialog);
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

        if (mShouldShowLogoAndSearchBox) {
            setUpLogo(activity, profile, mPreviewPropertyModel);
            setUpSearchBox(mPreviewPropertyModel, profile);
        }

        mDialog.show();
        NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewShow();
    }

    @Override
    public WindowInsetsCompat onApplyWindowInsets(
            View view, WindowInsetsCompat windowInsetsCompat) {
        Insets combinedInsets =
                windowInsetsCompat.getInsets(
                        WindowInsetsCompat.Type.systemBars()
                                | WindowInsetsCompat.Type.displayCutout());

        mPreviewPropertyModel.set(
                NtpThemeProperty.TOP_GUIDELINE_BEGIN, mToolBarHeight + combinedInsets.top);

        // Only applies padding for 3-button navigation. Gesture navigation should remain
        // edge-to-edge (0 padding).
        boolean hasTappableNavBar =
                EdgeToEdgeUtils.hasTappableNavigationBarFromInsets(windowInsetsCompat);
        int bottomInsetForPadding = hasTappableNavBar ? combinedInsets.bottom : 0;

        // Groups Left, Right, and Bottom into a Rect to update the model once. We pass 0 for top
        // since it's handled by the TOP_INSETS property above.
        Rect sideAndBottomInsets =
                new Rect(combinedInsets.left, 0, combinedInsets.right, bottomInsetForPadding);
        mPreviewPropertyModel.set(NtpThemeProperty.SIDE_AND_BOTTOM_INSETS, sideAndBottomInsets);

        if (!hasTappableNavBar) {
            // Since bottom padding is 0, the layout extends to the very bottom edge of the screen.
            // Elevates the buttons by adding the navigation bar height to their base margin,
            // preventing the gesture handle from overlapping the buttons.
            mPreviewPropertyModel.set(
                    BUTTON_BOTTOM_MARGIN, mButtonBottomMargin + combinedInsets.bottom);
        }
        // Consumes the insets since the root view already adjusted their paddings.
        return new WindowInsetsCompat.Builder(windowInsetsCompat)
                .setInsets(WindowInsetsCompat.Type.statusBars(), Insets.NONE)
                .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.NONE)
                .setInsets(WindowInsetsCompat.Type.displayCutout(), Insets.NONE)
                .build();
    }

    private void updateSearchBoxWidthPreview() {
        // Guards against rare cases where a layout update occurs after the bottom sheet has already
        // initiated the destruction of the crop view.
        if (mCropImageView == null) {
            return;
        }

        // 1. Computes the padding added to the feed section.
        Resources resources = mActivity.getResources();
        int totalFeedPaddingPerSide =
                FeedStreamViewResizerUtils.computePadding(
                        mActivity, mUiConfig, mCropImageView, mToolBarHeight);
        int compensation =
                FeedStreamViewResizerUtils.getFeedNtpCompensationMargin(resources, mUiConfig);
        int effectiveFeedPaddingTotal = (totalFeedPaddingPerSide + compensation) * 2;

        // 2. Computes the margin added to the ntp.
        // isTablet is hardcoded to false as this coordinator is guarded against creation on
        // tablets.
        int ntpMarginsTotal =
                getSearchBoxTwoSideMargin(resources, mUiConfig, /* isTablet= */ false);

        int finalWidth = mCropImageView.getWidth() - effectiveFeedPaddingTotal - ntpMarginsTotal;

        mPreviewPropertyModel.set(NtpThemeProperty.SEARCH_BOX_WIDTH, finalWidth);
    }

    /**
     * Records the user interaction metrics for the image preview. When the user rotates the screen
     * and pinches to resize together (the order doesn't matter), only {@link
     * PreviewInteractionType#ROTATE_SCREEN_AND_PINCH_TO_RESIZE} is recorded once. Individual {@link
     * PreviewInteractionType#ROTATE_SCREEN} and {@link PreviewInteractionType#PINCH_TO_RESIZE}
     * metrics are not recorded to avoid double counting.
     */
    private void recordPreviewInteractionsMetric() {
        assumeNonNull(mCropImageView);
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
                        mActivity.getResources(),
                        /* isLogoDoodle= */ logoBitmap != null,
                        LogoUtils.getDoodleSize(activity.isInMultiWindowMode())));
    }

    private void setUpSearchBox(PropertyModel propertyModel, Profile profile) {
        Resources resources = mActivity.getResources();
        boolean showSearchBoxTall =
                ComposeplateUtils.isComposeplateEnabled(/* isTablet= */ false, profile)
                        && ChromeFeatureList.sAndroidComposeplateV2Enabled.getValue();

        propertyModel.set(
                SEARCH_BOX_HEIGHT,
                NtpCustomizationUtils.getSearchBoxHeightWithShadows(
                        resources, showSearchBoxTall, /* hasShadowApplied= */ true));

        propertyModel.set(
                SEARCH_BOX_TOP_MARGIN,
                NtpCustomizationUtils.getLogoViewBottomMarginPx(
                        resources, /* applyShadow= */ true));
    }

    PropertyModel getPropertyModelForTesting() {
        return mPreviewPropertyModel;
    }

    public void destroy() {
        mPreviewPropertyModel.set(NtpThemeProperty.PREVIEW_SAVE_CLICK_LISTENER, null);
        mPreviewPropertyModel.set(NtpThemeProperty.PREVIEW_CANCEL_CLICK_LISTENER, null);
        if (mPreviewLayout != null && mLayoutChangeListener != null) {
            mPreviewLayout.removeOnLayoutChangeListener(mLayoutChangeListener);
            mLayoutChangeListener = null;
        }
        if (mCropImageView != null) {
            mCropImageView.destroy();
            mCropImageView = null;
        }
        mPreviewLayout = null;
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
        assumeNonNull(mCropImageView);
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

        // Records metrics before the callback closes the bottom sheet.
        NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewInteractions(
                PreviewInteractionType.SAVE);
        recordPreviewInteractionsMetric();

        onBottomSheetClickedCallback.onResult(true);
        dialog.dismiss();
    }

    void setCropImageViewForTesting(CropImageView view) {
        mCropImageView = view;
    }
}
