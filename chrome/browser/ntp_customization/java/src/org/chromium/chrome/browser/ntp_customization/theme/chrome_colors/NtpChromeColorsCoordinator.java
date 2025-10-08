// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.launchUriActivity;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.support.annotation.ColorInt;
import android.support.annotation.VisibleForTesting;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.ConstraintSet;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.List;

/** Coordinator for the NTP appearance chrome colors bottom sheet in the NTP customization. */
@NullMarked
public class NtpChromeColorsCoordinator {
    // TODO(crbug.com/423579377): Update the url for learn more button.
    private static final String LEARN_MORE_CLICK_URL =
            "https://support.google.com/chrome/?p=new_tab";
    private static final int MAX_NUMBER_OF_COLORS_PER_ROW = 7;
    private final List<NtpThemeColorInfo> mChromeColorsList = new ArrayList<>();
    private final View mBackButton;
    private final ImageView mLearnMoreButton;
    private final Context mContext;
    private final BottomSheetDelegate mDelegate;
    private final ColorGridView mChromeColorsRecyclerView;
    private final int mItemWidth;
    private final int mSpacing;
    private final Runnable mOnChromeColorSelectedCallback;
    private final @Nullable NtpThemeColorInfo mPrimaryColorInfo;
    private @Nullable EditText mBackgroundColorInput;
    private @Nullable EditText mPrimaryColorInput;
    private @Nullable ImageView mBackgroundColorCircleImageView;
    private @Nullable ImageView mPrimaryColorCircleImageView;
    private @Nullable ButtonCompat mSaveColorButton;
    private @Nullable @ColorInt Integer mTypedBackgroundColor;
    private @Nullable @ColorInt Integer mTypedPrimaryColor;

    /**
     * Constructor for the chrome colors coordinator.
     *
     * @param context The context for inflating views and accessing resources.
     * @param delegate The delegate to handle bottom sheet interactions.
     * @param onChromeColorSelectedCallback The callback to run when a color is selected.
     */
    public NtpChromeColorsCoordinator(
            Context context, BottomSheetDelegate delegate, Runnable onChromeColorSelectedCallback) {
        mContext = context;
        mDelegate = delegate;
        mOnChromeColorSelectedCallback = onChromeColorSelectedCallback;
        View ntpChromeColorsBottomSheetView =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.ntp_customization_chrome_colors_bottom_sheet_layout,
                                null,
                                false);

        delegate.registerBottomSheetLayout(CHROME_COLORS, ntpChromeColorsBottomSheetView);

        mBackButton = ntpChromeColorsBottomSheetView.findViewById(R.id.back_button);
        mBackButton.setOnClickListener(v -> delegate.showBottomSheet(THEME));

        mLearnMoreButton = ntpChromeColorsBottomSheetView.findViewById(R.id.learn_more_button);
        mLearnMoreButton.setOnClickListener(this::handleLearnMoreClick);
        if (ChromeFeatureList.sNewTabPageCustomizationV2ShowColorPicker.getValue()) {
            setupColorInputs(ntpChromeColorsBottomSheetView);
        }

        mChromeColorsRecyclerView =
                ntpChromeColorsBottomSheetView.findViewById(R.id.chrome_colors_recycler_view);
        mItemWidth =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.ntp_customization_back_button_clickable_size);
        mSpacing =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.ntp_customization_chrome_colors_grid_spacing);

        mPrimaryColorInfo = NtpCustomizationUtils.loadColorInfoFromSharedPreference(mContext);
        buildRecyclerView();
        setRecyclerViewMaxWidth(ntpChromeColorsBottomSheetView);
    }

    private void buildRecyclerView() {
        mChromeColorsRecyclerView.setLayoutManager(new GridLayoutManager(mContext, 1));
        mChromeColorsRecyclerView.init(mItemWidth, mSpacing);

        int primaryColorIndex =
                NtpThemeColorUtils.initColorsListAndFindPrimaryColorIndex(
                        mContext, mChromeColorsList, mPrimaryColorInfo);
        NtpChromeColorsAdapter adapter =
                new NtpChromeColorsAdapter(
                        mContext, mChromeColorsList, this::onItemClicked, primaryColorIndex);

        mChromeColorsRecyclerView.setAdapter(adapter);
    }

    private void setRecyclerViewMaxWidth(View ntpChromeColorsBottomSheetView) {
        ConstraintLayout constraintLayout = (ConstraintLayout) ntpChromeColorsBottomSheetView;
        FrameLayout recyclerViewContainer =
                ntpChromeColorsBottomSheetView.findViewById(
                        R.id.chrome_colors_recycler_view_container);

        int maxWidthPx = MAX_NUMBER_OF_COLORS_PER_ROW * (mItemWidth + mSpacing);

        ConstraintSet constraintSet = new ConstraintSet();
        constraintSet.clone(constraintLayout);
        constraintSet.constrainedWidth(recyclerViewContainer.getId(), true);
        constraintSet.constrainMaxWidth(recyclerViewContainer.getId(), maxWidthPx);
        constraintSet.applyTo(constraintLayout);
    }

    /**
     * Called when the item view is clicked.
     *
     * @param ntpThemeColorInfo The color instance that the user clicked.
     */
    @VisibleForTesting
    void onItemClicked(NtpThemeColorInfo ntpThemeColorInfo) {
        mDelegate.onNewColorSelected(
                !NtpThemeColorUtils.isPrimaryColorMatched(
                        mContext, mPrimaryColorInfo, ntpThemeColorInfo));
        @NtpBackgroundImageType
        int newType =
                ntpThemeColorInfo instanceof NtpThemeColorFromHexInfo
                        ? NtpBackgroundImageType.COLOR_FROM_HEX
                        : NtpBackgroundImageType.CHROME_COLOR;
        NtpCustomizationConfigManager.getInstance()
                .onBackgroundColorChanged(mContext, ntpThemeColorInfo, newType);
        mOnChromeColorSelectedCallback.run();
    }

    /** Cleans up the resources used by this coordinator. */
    public void destroy() {
        mBackButton.setOnClickListener(null);
        mLearnMoreButton.setOnClickListener(null);
        mChromeColorsList.clear();

        if (mBackgroundColorInput != null) {
            mBackgroundColorInput.setOnClickListener(null);
        }
        if (mPrimaryColorInput != null) {
            mPrimaryColorInput.setOnClickListener(null);
        }
        if (mSaveColorButton != null) {
            mSaveColorButton.setOnClickListener(null);
        }
    }

    /**
     * Handles the click event for the "Learn More" button in the Chrome Colors bottom sheet.
     *
     * @param view The view that was clicked.
     */
    @VisibleForTesting
    void handleLearnMoreClick(View view) {
        launchUriActivity(view.getContext(), LEARN_MORE_CLICK_URL);
    }

    /**
     * Sets up the color picker view.
     *
     * @param bottomSheetView The parent bottom sheet view.
     */
    private void setupColorInputs(View bottomSheetView) {
        mBackgroundColorCircleImageView =
                bottomSheetView.findViewById(R.id.background_color_circle);
        mBackgroundColorInput = bottomSheetView.findViewById(R.id.background_color_input);
        mPrimaryColorCircleImageView = bottomSheetView.findViewById(R.id.primary_color_circle);
        mPrimaryColorInput = bottomSheetView.findViewById(R.id.primary_color_input);
        mSaveColorButton = bottomSheetView.findViewById(R.id.save_color_button);
        mSaveColorButton.setOnClickListener(this::saveColors);
        assumeNonNull(mBackgroundColorInput)
                .addTextChangedListener(
                        new EmptyTextWatcher() {
                            @Override
                            public void onTextChanged(
                                    CharSequence charSequence, int i, int i1, int i2) {
                                mTypedBackgroundColor =
                                        updateColorCircle(
                                                charSequence.toString(),
                                                assumeNonNull(mBackgroundColorCircleImageView));
                            }
                        });

        assumeNonNull(mPrimaryColorInput)
                .addTextChangedListener(
                        new EmptyTextWatcher() {
                            @Override
                            public void onTextChanged(
                                    CharSequence charSequence, int i, int i1, int i2) {
                                mTypedPrimaryColor =
                                        updateColorCircle(
                                                charSequence.toString(),
                                                assumeNonNull(mPrimaryColorCircleImageView));
                            }
                        });
        View containerView = bottomSheetView.findViewById(R.id.custom_color_picker_container);
        containerView.setVisibility(View.VISIBLE);
    }

    /**
     * Updates the color of the color indicator view when a valid color hex is typed.
     *
     * @param hex The hexadecimal string of a color.
     * @param circleImageView The color indicator view.
     * @return The color as an int if the hexadecimal string is valid; otherwise, returns null.
     */
    @VisibleForTesting
    @Nullable
    @ColorInt
    Integer updateColorCircle(String hex, ImageView circleImageView) {
        if (hex.length() < 6) return null;

        String colorString = hex.trim();
        if (!colorString.startsWith("#")) {
            colorString = "#" + colorString;
        }

        @ColorInt int color = Color.parseColor(colorString);
        Drawable background = circleImageView.getBackground();
        if (background instanceof GradientDrawable) {
            ((GradientDrawable) background.mutate()).setColor(color);
            circleImageView.setVisibility(View.VISIBLE);
            return color;
        }
        return null;
    }

    /**
     * Called to save the manually set colors if both the background color and the primary color are
     * valid.
     */
    private void saveColors(View view) {
        if (mTypedBackgroundColor == null || mTypedPrimaryColor == null) return;

        NtpThemeColorInfo colorInfo =
                new NtpThemeColorFromHexInfo(
                        mContext, mTypedBackgroundColor.intValue(), mTypedPrimaryColor.intValue());
        onItemClicked(colorInfo);
    }

    public @Nullable NtpThemeColorInfo getPrimaryColorInfoForTesting() {
        return mPrimaryColorInfo;
    }

    public @Nullable ImageView getBackgroundColorCircleImageViewForTesting() {
        return mBackgroundColorCircleImageView;
    }

    /**
     * A RecyclerView that automatically adjusts its GridLayoutManager's span count based on its
     * measured width. This is more efficient than calculating in onLayoutChildren as it's done
     * during the measurement pass, preventing re-layout cycles.
     */
    public static class ColorGridView extends RecyclerView {
        private @Nullable GridLayoutManager mGridLayoutManager;
        private int mSpanCount;
        private int mItemWidth;
        private int mSpacing;
        private int mLastRecyclerViewWidth;

        public ColorGridView(Context context, @Nullable AttributeSet attrs) {
            super(context, attrs);
        }

        /** Initializes the grid with necessary dimensions for span calculation. */
        public void init(int itemWidth, int spacing) {
            mItemWidth = itemWidth;
            mSpacing = spacing;
        }

        @Override
        public void setLayoutManager(@Nullable LayoutManager layout) {
            super.setLayoutManager(layout);
            if (layout instanceof GridLayoutManager) {
                mGridLayoutManager = (GridLayoutManager) layout;
            }
        }

        @Override
        protected void onMeasure(int widthSpec, int heightSpec) {
            assert mGridLayoutManager != null;

            super.onMeasure(widthSpec, heightSpec);
            int measuredWidth = getMeasuredWidth();
            if (measuredWidth > 0 && mLastRecyclerViewWidth != measuredWidth) {
                mLastRecyclerViewWidth = measuredWidth;
                int totalItemSpace = mItemWidth + mSpacing;
                assert totalItemSpace > 0;

                int maxSpanCount = Math.max(1, measuredWidth / totalItemSpace);
                if (mSpanCount != maxSpanCount) {
                    mSpanCount = maxSpanCount;
                    mGridLayoutManager.setSpanCount(mSpanCount);
                }
            }
        }
    }
}
