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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeBridge;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeBridge.ThemeCollectionSelectionListener;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.GURL;

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
    private final List<CollectionImage> mThemeCollectionImageList = new ArrayList<>();
    private final Context mContext;
    private final View mNtpSingleThemeCollectionBottomSheetView;
    private final View mBackButton;
    private final ImageView mLearnMoreButton;
    private final TextView mTitle;
    private final RecyclerView mSingleThemeCollectionBottomSheetRecyclerView;
    private NtpThemeCollectionsAdapter mNtpThemeCollectionsAdapter;
    private final NtpThemeBridge mNtpThemeBridge;
    private final ImageFetcher mImageFetcher;
    private final BottomSheetDelegate mBottomSheetDelegate;
    private final ThemeCollectionSelectionListener mThemeCollectionSelectionListener;
    private final Runnable mOnThemeImageSelectedCallback;
    private final ComponentCallbacks mComponentCallbacks;
    private final int mItemMaxWidth;
    private final int mSpacing;
    private boolean mHasDisplayedBefore;
    private int mScreenWidth;

    /**
     * Constructor for the single theme collection coordinator.
     *
     * @param context The context for inflating views and accessing resources.
     * @param delegate The delegate to handle bottom sheet interactions.
     * @param ntpThemeBridge The bridge to fetch theme data from native.
     * @param imageFetcher The fetcher to retrieve images.
     * @param collectionId The ID of the current theme collection to display.
     * @param themeCollectionTitle The title of the current theme collection.
     * @param previousBottomSheetState The bottom sheet state in the previous theme collections
     *     bottom sheet.
     * @param onThemeImageSelectedCallback The callback to run when a theme image is selected.
     */
    NtpSingleThemeCollectionCoordinator(
            Context context,
            BottomSheetDelegate delegate,
            NtpThemeBridge ntpThemeBridge,
            ImageFetcher imageFetcher,
            String collectionId,
            String themeCollectionTitle,
            @SheetState int previousBottomSheetState,
            Runnable onThemeImageSelectedCallback) {
        mContext = context;
        mBottomSheetDelegate = delegate;
        mNtpThemeBridge = ntpThemeBridge;
        mImageFetcher = imageFetcher;
        mThemeCollectionId = collectionId;
        mThemeCollectionTitle = themeCollectionTitle;
        mOnThemeImageSelectedCallback = onThemeImageSelectedCallback;

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

    void destroy() {
        if (mComponentCallbacks != null) {
            mContext.unregisterComponentCallbacks(mComponentCallbacks);
        }

        mBackButton.setOnClickListener(null);
        mLearnMoreButton.setOnClickListener(null);

        if (mNtpThemeCollectionsAdapter != null) {
            mNtpThemeCollectionsAdapter.clearOnClickListeners();
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

    /**
     * Updates the single theme collection bottom sheet based on the given theme collection type.
     */
    void updateThemeCollection(
            String collectionId,
            String themeCollectionTitle,
            @SheetState int previousBottomSheetState) {
        if (mThemeCollectionTitle.equals(themeCollectionTitle)) {
            return;
        }

        mThemeCollectionId = collectionId;
        mThemeCollectionTitle = themeCollectionTitle;

        mTitle.setText(mThemeCollectionTitle);
        fetchImagesForCollection(previousBottomSheetState);
    }

    private void handleThemeCollectionImageClick(View view) {
        int position = mSingleThemeCollectionBottomSheetRecyclerView.getChildAdapterPosition(view);
        if (position == RecyclerView.NO_POSITION) return;

        CollectionImage image = mThemeCollectionImageList.get(position);

        // TODO(crbug.com/423579377): This will trigger the notification to all listeners, updating
        // both adapters. Should be updated to the service.
        mNtpThemeBridge.setSelectedTheme(image.collectionId, image.imageUrl);
        mOnThemeImageSelectedCallback.run();
    }

    private void handleLearnMoreClick(View view) {
        launchUriActivity(view.getContext(), LEARN_MORE_CLICK_URL);
    }

    /**
     * Fetches the images for the current collection and updates the adapter.
     *
     * @param previousBottomSheetState The bottom sheet state in the previous theme collections
     *     bottom sheet.
     */
    private void fetchImagesForCollection(@SheetState int previousBottomSheetState) {
        mNtpThemeBridge.getBackgroundImages(
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
                                mNtpThemeBridge.getSelectedThemeCollectionId(),
                                mNtpThemeBridge.getSelectedThemeCollectionImageUrl());
                    }

                    mHasDisplayedBefore = true;
                });
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
