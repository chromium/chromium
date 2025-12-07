// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.SINGLE_THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.launchUriActivity;
import static org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionsItemType.SINGLE_THEME_COLLECTION_ITEM;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationMetricsUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.components.image_fetcher.ImageFetcher;

import java.util.ArrayList;
import java.util.List;

/**
 * Coordinator for the NTP appearance single theme collection bottom sheet in the NTP customization.
 */
@NullMarked
public class NtpSingleThemeCollectionCoordinator {
    // TODO(crbug.com/423579377): Update the url for learn more button.
    private static final String LEARN_MORE_CLICK_URL =
            "https://support.google.com/chrome/?p=new_tab";
    private static final int RECYCLE_VIEW_SPAN_COUNT = 3;

    private String mThemeCollectionId;
    private String mThemeCollectionTitle;
    private int mThemeCollectionHash;
    private final List<CollectionImage> mThemeCollectionImageList = new ArrayList<>();
    private final Context mContext;
    private final View mNtpSingleThemeCollectionBottomSheetView;
    private final View mBackButton;
    private final ImageView mLearnMoreButton;
    private final TextView mTitle;
    private final MaterialSwitchWithText mDailyRefreshSwitchButton;
    private final RecyclerView mSingleThemeCollectionBottomSheetRecyclerView;
    private NtpThemeCollectionsAdapter mNtpThemeCollectionsAdapter;
    private final NtpThemeCollectionManager mNtpThemeCollectionManager;
    private final ImageFetcher mImageFetcher;
    private final BottomSheetDelegate mBottomSheetDelegate;
    private final ComponentCallbacks mComponentCallbacks;
    private final int mItemMaxWidth;
    private final int mSpacing;
    private final Runnable mOnDailyRefreshCancelledCallback;
    private boolean mHasDisplayedBefore;
    private int mScreenWidth;
    private boolean mIsThemeCollectionSelected;

    /**
     * Constructor for the single theme collection coordinator.
     *
     * @param context The context for inflating views and accessing resources.
     * @param delegate The delegate to handle bottom sheet interactions.
     * @param ntpThemeCollectionManager The manager to fetch theme data from native.
     * @param imageFetcher The fetcher to retrieve images.
     * @param collectionId The ID of the current theme collection to display.
     * @param themeCollectionTitle The title of the current theme collection.
     * @param previousBottomSheetState The bottom sheet state in the previous theme collections
     *     bottom sheet.
     * @param onDailyRefreshCancelledCallback The callback to run when daily refresh is cancelled.
     */
    NtpSingleThemeCollectionCoordinator(
            Context context,
            BottomSheetDelegate delegate,
            NtpThemeCollectionManager ntpThemeCollectionManager,
            ImageFetcher imageFetcher,
            String collectionId,
            String themeCollectionTitle,
            int themeCollectionHash,
            @SheetState int previousBottomSheetState,
            Runnable onDailyRefreshCancelledCallback) {
        mContext = context;
        mBottomSheetDelegate = delegate;
        mNtpThemeCollectionManager = ntpThemeCollectionManager;
        mImageFetcher = imageFetcher;
        mThemeCollectionId = collectionId;
        mThemeCollectionTitle = themeCollectionTitle;
        mThemeCollectionHash = themeCollectionHash;
        mOnDailyRefreshCancelledCallback = onDailyRefreshCancelledCallback;

        mItemMaxWidth =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.ntp_customization_theme_collections_list_item_max_width);
        mSpacing =
                context.getResources()
                                .getDimensionPixelSize(
                                        R.dimen
                                                .ntp_customization_theme_collection_list_item_padding_horizontal)
                        * 2;

        mNtpSingleThemeCollectionBottomSheetView =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout
                                        .ntp_customization_single_theme_collection_bottom_sheet_layout,
                                null,
                                false);

        mBottomSheetDelegate.registerBottomSheetLayout(
                SINGLE_THEME_COLLECTION, mNtpSingleThemeCollectionBottomSheetView);

        // Add the back press handler of the back button in the bottom sheet.
        mBackButton = mNtpSingleThemeCollectionBottomSheetView.findViewById(R.id.back_button);
        mBackButton.setOnClickListener(
                v -> mBottomSheetDelegate.showBottomSheet(THEME_COLLECTIONS));

        // Manage the learn more button in the theme collections bottom sheet.
        mLearnMoreButton =
                mNtpSingleThemeCollectionBottomSheetView.findViewById(R.id.learn_more_button);
        mLearnMoreButton.setOnClickListener(this::handleLearnMoreClick);

        // Update the title of the bottom sheet.
        mTitle = mNtpSingleThemeCollectionBottomSheetView.findViewById(R.id.bottom_sheet_title);
        mTitle.setText(mThemeCollectionTitle);

        // Update the daily refresh switch of the bottom sheet.
        mDailyRefreshSwitchButton =
                mNtpSingleThemeCollectionBottomSheetView.findViewById(
                        R.id.daily_update_switch_button);
        setDailyRefreshSwitchButtonStatus();
        mDailyRefreshSwitchButton.setOnCheckedChangeListener(this::handleDailyRefreshClick);

        // Build the RecyclerView containing the images of this particular theme collection in the
        // bottom sheet.
        mSingleThemeCollectionBottomSheetRecyclerView =
                mNtpSingleThemeCollectionBottomSheetView.findViewById(
                        R.id.single_theme_collection_recycler_view);
        GridLayoutManager gridLayoutManager =
                new GridLayoutManager(context, RECYCLE_VIEW_SPAN_COUNT);
        mSingleThemeCollectionBottomSheetRecyclerView.setLayoutManager(gridLayoutManager);
        mNtpThemeCollectionsAdapter =
                new NtpThemeCollectionsAdapter(
                        mThemeCollectionImageList,
                        SINGLE_THEME_COLLECTION_ITEM,
                        this::handleThemeCollectionImageClick,
                        mImageFetcher);
        mSingleThemeCollectionBottomSheetRecyclerView.setAdapter(mNtpThemeCollectionsAdapter);

        NtpThemeCollectionsUtils.updateSpanCountOnLayoutChange(
                gridLayoutManager,
                mSingleThemeCollectionBottomSheetRecyclerView,
                mItemMaxWidth,
                mSpacing);
        mComponentCallbacks =
                NtpThemeCollectionsUtils.registerOrientationListener(
                        mContext,
                        (newConfig) ->
                                handleConfigurationChanged(
                                        newConfig,
                                        gridLayoutManager,
                                        mSingleThemeCollectionBottomSheetRecyclerView));

        // Fetches the images for the current collection.
        fetchImagesForCollection(previousBottomSheetState);
    }

    void destroy() {
        if (mComponentCallbacks != null) {
            mContext.unregisterComponentCallbacks(mComponentCallbacks);
        }

        mBackButton.setOnClickListener(null);
        mLearnMoreButton.setOnClickListener(null);

        if (mNtpThemeCollectionsAdapter != null) {
            mNtpThemeCollectionsAdapter.clearOnClickListeners();
        }
    }

    /**
     * Initialize the bottom sheet content when it becomes visible, including the selection state of
     * the theme collections and the status of the daily refresh switch.
     */
    void initializeBottomSheetContent() {
        if (mNtpThemeCollectionsAdapter != null) {
            mNtpThemeCollectionsAdapter.setSelection(
                    mNtpThemeCollectionManager.getSelectedThemeCollectionId(),
                    mNtpThemeCollectionManager.getSelectedThemeCollectionImageUrl());
        }
        setDailyRefreshSwitchButtonStatus();
    }

    /**
     * Handles configuration changes, particularly screen width changes, to update the span count of
     * the grid layout.
     *
     * @param newConfig The new configuration.
     * @param manager The {@link GridLayoutManager} for the RecyclerView.
     * @param recyclerView The {@link RecyclerView} whose span count needs to be updated.
     */
    private void handleConfigurationChanged(
            Configuration newConfig, GridLayoutManager manager, RecyclerView recyclerView) {
        int currentScreenWidth = newConfig.screenWidthDp;
        if (currentScreenWidth == mScreenWidth) {
            return;
        }

        mScreenWidth = currentScreenWidth;
        NtpThemeCollectionsUtils.updateSpanCountOnLayoutChange(
                manager, recyclerView, mItemMaxWidth, mSpacing);
    }

    /**
     * Updates the single theme collection bottom sheet based on the given theme collection type.
     */
    void updateThemeCollection(
            String collectionId,
            String themeCollectionTitle,
            int themeCollectionHash,
            @SheetState int previousBottomSheetState) {
        if (mThemeCollectionTitle.equals(themeCollectionTitle)) {
            return;
        }

        mThemeCollectionId = collectionId;
        mThemeCollectionTitle = themeCollectionTitle;
        mThemeCollectionHash = themeCollectionHash;
        mIsThemeCollectionSelected = false;

        mTitle.setText(mThemeCollectionTitle);
        fetchImagesForCollection(previousBottomSheetState);
    }

    private void handleThemeCollectionImageClick(View view) {
        int position = mSingleThemeCollectionBottomSheetRecyclerView.getChildAdapterPosition(view);
        if (position == RecyclerView.NO_POSITION) return;

        CollectionImage image = mThemeCollectionImageList.get(position);
        mNtpThemeCollectionManager.setThemeCollectionImage(image);
        if (!mIsThemeCollectionSelected) {
            NtpCustomizationMetricsUtils.recordThemeCollectionSelected(mThemeCollectionHash);
            mIsThemeCollectionSelected = true;
        }
    }

    private void handleLearnMoreClick(View view) {
        launchUriActivity(view.getContext(), LEARN_MORE_CLICK_URL);
    }

    /** Handles clicks on the daily refresh switch. */
    private void handleDailyRefreshClick(View view, Boolean isChecked) {
        if (isChecked) {
            mNtpThemeCollectionManager.setThemeCollectionDailyRefreshed(mThemeCollectionId);
        } else {
            // If unchecked, resets to the default background by invoking the callback.
            mOnDailyRefreshCancelledCallback.run();
        }
    }

    /**
     * Fetches the images for the current collection and updates the adapter.
     *
     * @param previousBottomSheetState The bottom sheet state in the previous theme collections
     *     bottom sheet.
     */
    private void fetchImagesForCollection(@SheetState int previousBottomSheetState) {
        mNtpThemeCollectionManager.getBackgroundImages(
                mThemeCollectionId,
                (images) -> {
                    mThemeCollectionImageList.clear();
                    if (images != null
                            && !images.isEmpty()
                            && mThemeCollectionId.equals(images.get(0).collectionId)) {
                        mThemeCollectionImageList.addAll(images);
                    }
                    mNtpThemeCollectionsAdapter.setItems(mThemeCollectionImageList);

                    if (previousBottomSheetState == SheetState.HALF || !mHasDisplayedBefore) {
                        // The single theme collection bottom sheet will be shown in a half state if
                        // it's either displayed for the first time or if the previous theme
                        // collections bottom sheet was in a half state.
                        mBottomSheetDelegate.getBottomSheetController().expandSheet();
                    }

                    if (!mHasDisplayedBefore) {
                        // After setting items, apply the current selection from the manager.
                        mNtpThemeCollectionsAdapter.setSelection(
                                mNtpThemeCollectionManager.getSelectedThemeCollectionId(),
                                mNtpThemeCollectionManager.getSelectedThemeCollectionImageUrl());
                        mHasDisplayedBefore = true;
                    }
                });
    }

    /** Sets the status of the daily refresh button. */
    private void setDailyRefreshSwitchButtonStatus() {
        boolean isChecked =
                mNtpThemeCollectionManager.getSelectedThemeCollectionId() != null
                        && mNtpThemeCollectionManager
                                .getSelectedThemeCollectionId()
                                .equals(mThemeCollectionId)
                        && mNtpThemeCollectionManager.getIsDailyRefreshEnabled();

        // Temporarily detach the listener to prevent onCheckedChanged from being triggered
        // unnecessarily.
        mDailyRefreshSwitchButton.setOnCheckedChangeListener(null);
        mDailyRefreshSwitchButton.setChecked(isChecked);
        mDailyRefreshSwitchButton.setOnCheckedChangeListener(this::handleDailyRefreshClick);
    }

    NtpThemeCollectionsAdapter getNtpThemeCollectionsAdapterForTesting() {
        return mNtpThemeCollectionsAdapter;
    }

    TextView getTitleForTesting() {
        return mTitle;
    }

    void setNtpThemeCollectionsAdapterForTesting(NtpThemeCollectionsAdapter adapter) {
        mNtpThemeCollectionsAdapter = adapter;
        mSingleThemeCollectionBottomSheetRecyclerView.setAdapter(adapter);
    }

    int getScreenWidthForTesting() {
        return mScreenWidth;
    }
}
