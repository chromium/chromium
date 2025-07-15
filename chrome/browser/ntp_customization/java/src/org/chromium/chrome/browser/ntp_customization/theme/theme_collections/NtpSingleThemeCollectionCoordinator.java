// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.SINGLE_THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.launchUriActivity;
import static org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionsItemType.SINGLE_THEME_COLLECTION_ITEM;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.R;

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

    private String mThemeCollectionTitle;
    private final List<Integer> mThemeCollectionImageList = new ArrayList<>();
    private final View mNtpSingleThemeCollectionBottomSheetView;
    private final View mBackButton;
    private final ImageView mLearnMoreButton;
    private final TextView mTitle;
    private final RecyclerView mSingleThemeCollectionBottomSheetRecyclerView;
    private NtpThemeCollectionsAdapter mNtpThemeCollectionsAdapter;

    NtpSingleThemeCollectionCoordinator(
            Context context, BottomSheetDelegate delegate, String themeCollectionTitle) {
        mThemeCollectionTitle = themeCollectionTitle;

        mNtpSingleThemeCollectionBottomSheetView =
                LayoutInflater.from(context)
                        .inflate(
                                R.layout
                                        .ntp_customization_single_theme_collection_bottom_sheet_layout,
                                null,
                                false);

        delegate.registerBottomSheetLayout(
                SINGLE_THEME_COLLECTION, mNtpSingleThemeCollectionBottomSheetView);

        // Add the back press handler of the back button in the bottom sheet.
        mBackButton = mNtpSingleThemeCollectionBottomSheetView.findViewById(R.id.back_button);
        mBackButton.setOnClickListener(v -> delegate.showBottomSheet(THEME_COLLECTIONS));

        // Manage the learn more button in the theme collections bottom sheet.
        mLearnMoreButton =
                mNtpSingleThemeCollectionBottomSheetView.findViewById(R.id.learn_more_button);
        mLearnMoreButton.setOnClickListener(this::handleLearnMoreClick);

        // Update the title of the bottom sheet.
        mTitle = mNtpSingleThemeCollectionBottomSheetView.findViewById(R.id.bottom_sheet_title);
        mTitle.setText(mThemeCollectionTitle);

        // TODO(crbug.com/423579377): Generate this theme collection images list.

        // Build the RecyclerView containing the images of this particular theme collection in the
        // bottom sheet.
        mSingleThemeCollectionBottomSheetRecyclerView =
                mNtpSingleThemeCollectionBottomSheetView.findViewById(
                        R.id.single_theme_collection_recycler_view);
        mSingleThemeCollectionBottomSheetRecyclerView.setLayoutManager(
                new GridLayoutManager(context, /* spanCount= */ 3));
        mNtpThemeCollectionsAdapter =
                new NtpThemeCollectionsAdapter(
                        mThemeCollectionImageList, SINGLE_THEME_COLLECTION_ITEM, null);
        mSingleThemeCollectionBottomSheetRecyclerView.setAdapter(mNtpThemeCollectionsAdapter);
    }

    void destroy() {
        mBackButton.setOnClickListener(null);
        mLearnMoreButton.setOnClickListener(null);

        if (mNtpThemeCollectionsAdapter != null) {
            mNtpThemeCollectionsAdapter.clearOnClickListeners();
        }
    }

    /**
     * Updates the single theme collection bottom sheet based on the given theme collection type.
     */
    void updateThemeCollection(String themeCollectionTitle) {
        if (mThemeCollectionTitle.equals(themeCollectionTitle)) {
            return;
        }

        mThemeCollectionTitle = themeCollectionTitle;
        mTitle.setText(mThemeCollectionTitle);

        mThemeCollectionImageList.clear();
        // TODO(crbug.com/423579377): Generate this theme collection images list.
        mNtpThemeCollectionsAdapter.setItems(mThemeCollectionImageList);
    }

    void handleLearnMoreClick(View view) {
        launchUriActivity(view.getContext(), LEARN_MORE_CLICK_URL);
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
}
