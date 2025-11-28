// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.SINGLE_THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.launchUriActivity;
import static org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionsItemType.THEME_COLLECTIONS_ITEM;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationMetricsUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.image_fetcher.ImageFetcher;

import java.util.ArrayList;
import java.util.List;

/** Coordinator for the NTP appearance theme collections bottom sheet in the NTP customization. */
@NullMarked
public class NtpThemeCollectionsCoordinator {
    // TODO(crbug.com/423579377): Update the url for learn more button.
    private static final String LEARN_MORE_CLICK_URL =
            "https://support.google.com/chrome/?p=new_tab";
    private static final int RECYCLE_VIEW_SPAN_COUNT = 3;

    private final List<BackgroundCollection> mThemeCollectionsList = new ArrayList<>();
    private final BottomSheetDelegate mBottomSheetDelegate;
    private final Context mContext;
    private final View mNtpThemeCollectionsBottomSheetView;
    private final View mBackButton;
    private final ImageView mLearnMoreButton;
    private final RecyclerView mThemeCollectionsBottomSheetRecyclerView;
    private final NtpThemeCollectionManager mNtpThemeCollectionManager;
    private final ImageFetcher mImageFetcher;
    private final ComponentCallbacks mComponentCallbacks;
    private final int mItemMaxWidth;
    private final int mSpacing;
    private final Runnable mOnDailyRefreshCancelledCallback;
    private NtpThemeCollectionsAdapter mNtpThemeCollectionsAdapter;
    private int mScreenWidth;
    private @Nullable NtpSingleThemeCollectionCoordinator mNtpSingleThemeCollectionCoordinator;

    /**
     * Constructor for the coordinator.
     *
     * @param context The context for inflating views and accessing resources.
     * @param delegate The delegate to handle bottom sheet interactions.
     * @param profile The profile for which this coordinator is created.
     * @param ntpThemeCollectionManager The manager to fetch theme data.
     * @param onDailyRefreshCancelledCallback The callback for daily refresh function being
     *     cancelled.
     */
    public NtpThemeCollectionsCoordinator(
            Context context,
            BottomSheetDelegate delegate,
            Profile profile,
            NtpThemeCollectionManager ntpThemeCollectionManager,
            Runnable onDailyRefreshCancelledCallback) {
        mContext = context;
        mBottomSheetDelegate = delegate;
        mImageFetcher = NtpCustomizationUtils.createImageFetcher(profile);
        mNtpThemeCollectionManager = ntpThemeCollectionManager;
        mOnDailyRefreshCancelledCallback = onDailyRefreshCancelledCallback;

        mItemMaxWidth =
                mContext.getResources()
                        .getDimensionPixelSize(
                                R.dimen.ntp_customization_theme_collections_list_item_max_width);
        mSpacing =
                mContext.getResources()
                                .getDimensionPixelSize(
                                        R.dimen
                                                .ntp_customization_theme_collection_list_item_padding_horizontal)
                        * 2;

        mNtpThemeCollectionsBottomSheetView =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout.ntp_customization_theme_collections_bottom_sheet_layout,
                                null,
                                false);

        mBottomSheetDelegate.registerBottomSheetLayout(
                THEME_COLLECTIONS, mNtpThemeCollectionsBottomSheetView);

        // Add the back press handler of the back button in the bottom sheet.
        mBackButton = mNtpThemeCollectionsBottomSheetView.findViewById(R.id.back_button);
        mBackButton.setOnClickListener(v -> mBottomSheetDelegate.showBottomSheet(THEME));

        // Manage the learn more button in the theme collections bottom sheet.
        mLearnMoreButton = mNtpThemeCollectionsBottomSheetView.findViewById(R.id.learn_more_button);
        mLearnMoreButton.setOnClickListener(this::handleLearnMoreClick);

        // Build the RecyclerView containing theme collections in the bottom sheet.
        mThemeCollectionsBottomSheetRecyclerView =
                mNtpThemeCollectionsBottomSheetView.findViewById(
                        R.id.theme_collections_recycler_view);
        GridLayoutManager gridLayoutManager =
                new GridLayoutManager(context, RECYCLE_VIEW_SPAN_COUNT);
        mThemeCollectionsBottomSheetRecyclerView.setLayoutManager(gridLayoutManager);
        mNtpThemeCollectionsAdapter =
                new NtpThemeCollectionsAdapter(
                        mThemeCollectionsList,
                        THEME_COLLECTIONS_ITEM,
                        this::handleThemeCollectionClick,
                        mImageFetcher);
        mThemeCollectionsBottomSheetRecyclerView.setAdapter(mNtpThemeCollectionsAdapter);

        NtpThemeCollectionsUtils.updateSpanCountOnLayoutChange(
                gridLayoutManager,
                mThemeCollectionsBottomSheetRecyclerView,
                mItemMaxWidth,
                mSpacing);
        mComponentCallbacks =
                NtpThemeCollectionsUtils.registerOrientationListener(
                        mContext,
                        (newConfig) ->
                                handleConfigurationChanged(
                                        newConfig,
                                        gridLayoutManager,
                                        mThemeCollectionsBottomSheetRecyclerView));

        // Fetches the theme collections.
        mNtpThemeCollectionManager.getBackgroundCollections(
                (collections) -> {
                    mThemeCollectionsList.clear();
                    if (collections != null) {
                        mThemeCollectionsList.addAll(collections);
                    }
                    mNtpThemeCollectionsAdapter.setItems(mThemeCollectionsList);
                    // Once items are loaded, expand to the half state.
                    delegate.getBottomSheetController().expandSheet();

                    // After setting items, apply the current selection from the manager.
                    mNtpThemeCollectionsAdapter.setSelection(
                            mNtpThemeCollectionManager.getSelectedThemeCollectionId(),
                            mNtpThemeCollectionManager.getSelectedThemeCollectionImageUrl());
                });
    }

    public void destroy() {
        if (mComponentCallbacks != null) {
            mContext.unregisterComponentCallbacks(mComponentCallbacks);
        }

        mImageFetcher.destroy();

        mBackButton.setOnClickListener(null);
        mLearnMoreButton.setOnClickListener(null);

        if (mNtpThemeCollectionsAdapter != null) {
            mNtpThemeCollectionsAdapter.clearOnClickListeners();
        }

        if (mNtpSingleThemeCollectionCoordinator != null) {
            mNtpSingleThemeCollectionCoordinator.destroy();
        }
    }

    /**
     * Initialize the bottom sheet content of the given bottom sheet type when it becomes visible.
     *
     * @param bottomSheetType The type of the bottom sheet to update.
     */
    public void initializeBottomSheetContent(@BottomSheetType int bottomSheetType) {
        switch (bottomSheetType) {
            case THEME_COLLECTIONS:
                if (mNtpThemeCollectionsAdapter != null) {
                    mNtpThemeCollectionsAdapter.setSelection(
                            mNtpThemeCollectionManager.getSelectedThemeCollectionId(),
                            mNtpThemeCollectionManager.getSelectedThemeCollectionImageUrl());
                }
                return;
            case SINGLE_THEME_COLLECTION:
                if (mNtpSingleThemeCollectionCoordinator != null) {
                    mNtpSingleThemeCollectionCoordinator.initializeBottomSheetContent();
                }
                return;
            default:
                assert false : "Bottom sheet type not supported!";
        }
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

    private void handleThemeCollectionClick(View view) {
        int position = mThemeCollectionsBottomSheetRecyclerView.getChildAdapterPosition(view);

        if (position == RecyclerView.NO_POSITION) return;

        BackgroundCollection collection = mThemeCollectionsList.get(position);
        String collectionId = collection.id;
        String themeCollectionTitle = collection.label;
        int themeCollectionHash = collection.hash;

        @SheetState
        int currentBottomSheetState =
                mBottomSheetDelegate.getBottomSheetController().getSheetState();
        if (mNtpSingleThemeCollectionCoordinator != null) {
            mNtpSingleThemeCollectionCoordinator.updateThemeCollection(
                    collectionId,
                    themeCollectionTitle,
                    themeCollectionHash,
                    currentBottomSheetState);
        } else {
            mNtpSingleThemeCollectionCoordinator =
                    new NtpSingleThemeCollectionCoordinator(
                            mContext,
                            mBottomSheetDelegate,
                            mNtpThemeCollectionManager,
                            mImageFetcher,
                            collectionId,
                            themeCollectionTitle,
                            themeCollectionHash,
                            currentBottomSheetState,
                            mOnDailyRefreshCancelledCallback);
        }

        mBottomSheetDelegate.showBottomSheet(BottomSheetType.SINGLE_THEME_COLLECTION);
        NtpCustomizationMetricsUtils.recordThemeCollectionShow(themeCollectionHash);
    }

    private void handleLearnMoreClick(View view) {
        launchUriActivity(view.getContext(), LEARN_MORE_CLICK_URL);
    }

    void setNtpThemeCollectionsAdapterForTesting(NtpThemeCollectionsAdapter adapter) {
        mNtpThemeCollectionsAdapter = adapter;
        mThemeCollectionsBottomSheetRecyclerView.setAdapter(adapter);
    }

    void setNtpSingleThemeCollectionCoordinatorForTesting(
            NtpSingleThemeCollectionCoordinator ntpSingleThemeCollectionCoordinator) {
        mNtpSingleThemeCollectionCoordinator = ntpSingleThemeCollectionCoordinator;
    }

    @Nullable NtpSingleThemeCollectionCoordinator
            getNtpSingleThemeCollectionCoordinatorForTesting() {
        return mNtpSingleThemeCollectionCoordinator;
    }

    NtpThemeCollectionManager getNtpThemeManagerForTesting() {
        return mNtpThemeCollectionManager;
    }

    int getScreenWidthForTesting() {
        return mScreenWidth;
    }
}
