// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

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
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeBridge;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeBridge.ThemeCollectionSelectionListener;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.url.GURL;

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
    private final NtpThemeBridge mNtpThemeBridge;
    private final ImageFetcher mImageFetcher;
    private final ThemeCollectionSelectionListener mThemeCollectionSelectionListener;
    private final Runnable mOnThemeImageSelectedCallback;
    private final ComponentCallbacks mComponentCallbacks;
    private final int mItemMaxWidth;
    private final int mSpacing;
    private NtpThemeCollectionsAdapter mNtpThemeCollectionsAdapter;
    private int mScreenWidth;
    private @Nullable NtpSingleThemeCollectionCoordinator mNtpSingleThemeCollectionCoordinator;

    /**
     * Constructor for the coordinator.
     *
     * @param context The context for inflating views and accessing resources.
     * @param delegate The delegate to handle bottom sheet interactions.
     * @param profile The profile for which this coordinator is created.
     * @param onThemeImageSelectedCallback The callback to run when a theme image is selected.
     */
    public NtpThemeCollectionsCoordinator(
            Context context,
            BottomSheetDelegate delegate,
            Profile profile,
            Runnable onThemeImageSelectedCallback) {
        mContext = context;
        mBottomSheetDelegate = delegate;
        mOnThemeImageSelectedCallback = onThemeImageSelectedCallback;

        mNtpThemeBridge = new NtpThemeBridge(profile);
        mImageFetcher =
                ImageFetcherFactory.createImageFetcher(
                        ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                        profile.getProfileKey(),
                        GlobalDiscardableReferencePool.getReferencePool());

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
        mNtpThemeBridge.getBackgroundCollections(
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
                            mNtpThemeBridge.getSelectedThemeCollectionId(),
                            mNtpThemeBridge.getSelectedThemeCollectionImageUrl());
                });

        mThemeCollectionSelectionListener =
                new ThemeCollectionSelectionListener() {
                    @Override
                    public void onThemeCollectionSelectionChanged(
                            @Nullable String themeCollectionId,
                            @Nullable GURL themeCollectionImageUrl) {
                        if (mNtpThemeCollectionsAdapter != null) {
                            mNtpThemeCollectionsAdapter.setSelection(
                                    themeCollectionId, themeCollectionImageUrl);
                        }
                    }
                };
        mNtpThemeBridge.addListener(mThemeCollectionSelectionListener);
    }

    public void destroy() {
        if (mComponentCallbacks != null) {
            mContext.unregisterComponentCallbacks(mComponentCallbacks);
        }

        mNtpThemeBridge.destroy();
        mImageFetcher.destroy();

        mBackButton.setOnClickListener(null);
        mLearnMoreButton.setOnClickListener(null);

        if (mNtpThemeCollectionsAdapter != null) {
            mNtpThemeCollectionsAdapter.clearOnClickListeners();
        }

        if (mNtpSingleThemeCollectionCoordinator != null) {
            mNtpSingleThemeCollectionCoordinator.destroy();
        }

        mNtpThemeBridge.removeListener(mThemeCollectionSelectionListener);
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

        @SheetState
        int currentBottomSheetState =
                mBottomSheetDelegate.getBottomSheetController().getSheetState();
        if (mNtpSingleThemeCollectionCoordinator != null) {
            mNtpSingleThemeCollectionCoordinator.updateThemeCollection(
                    collectionId, themeCollectionTitle, currentBottomSheetState);
        } else {
            mNtpSingleThemeCollectionCoordinator =
                    new NtpSingleThemeCollectionCoordinator(
                            mContext,
                            mBottomSheetDelegate,
                            mNtpThemeBridge,
                            mImageFetcher,
                            collectionId,
                            themeCollectionTitle,
                            currentBottomSheetState,
                            mOnThemeImageSelectedCallback);
        }

        mBottomSheetDelegate.showBottomSheet(BottomSheetType.SINGLE_THEME_COLLECTION);
    }

    private void handleLearnMoreClick(View view) {
        launchUriActivity(view.getContext(), LEARN_MORE_CLICK_URL);
    }

    /** Clears the theme collection selection. */
    public void clearThemeCollectionSelection() {
        mNtpThemeBridge.setSelectedTheme(
                /* themeCollectionId= */ null, /* themeCollectionImageUrl= */ null);
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

    NtpThemeBridge getNtpThemeBridgeForTesting() {
        return mNtpThemeBridge;
    }

    int getScreenWidthForTesting() {
        return mScreenWidth;
    }
}
