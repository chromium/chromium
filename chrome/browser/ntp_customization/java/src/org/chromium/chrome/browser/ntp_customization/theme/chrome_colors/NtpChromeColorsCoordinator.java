// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.launchUriActivity;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.ConstraintSet;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.R;

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
    private final ColorGridView mChromeColorsRecyclerView;
    private final int mItemWidth;
    private final int mSpacing;

    public NtpChromeColorsCoordinator(Context context, BottomSheetDelegate delegate) {
        mContext = context;
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

        buildRecyclerView();
        setRecyclerViewMaxWidth(ntpChromeColorsBottomSheetView);
    }

    private void buildRecyclerView() {
        mChromeColorsRecyclerView.setLayoutManager(new GridLayoutManager(mContext, 1));
        mChromeColorsRecyclerView.init(mItemWidth, mSpacing);

        initColorsList();
        NtpChromeColorsAdapter adapter =
                new NtpChromeColorsAdapter(mContext, mChromeColorsList, this::onItemClicked);
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

    private void initColorsList() {
        if (!mChromeColorsList.isEmpty()) return;

        mChromeColorsList.addAll(NtpThemeColorUtils.createThemeColorList(mContext));
    }

    /**
     * Called when the item view is clicked.
     *
     * @param ntpThemeColorInfo The color instance that the user clicked.
     */
    private void onItemClicked(NtpThemeColorInfo ntpThemeColorInfo) {
        NtpCustomizationConfigManager.getInstance()
                .onBackgroundColorChanged(
                        mContext,
                        ntpThemeColorInfo,
                        NtpCustomizationUtils.NtpBackgroundImageType.CHROME_COLOR);
    }

    /** Cleans up the resources used by this coordinator. */
    public void destroy() {
        mBackButton.setOnClickListener(null);
        mLearnMoreButton.setOnClickListener(null);
        mChromeColorsList.clear();
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
