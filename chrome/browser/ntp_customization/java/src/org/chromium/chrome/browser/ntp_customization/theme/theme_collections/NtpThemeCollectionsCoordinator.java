// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.launchUriActivity;
import static org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionsItemType.THEME_COLLECTIONS_ITEM;

import android.content.Context;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.R;

import java.util.ArrayList;
import java.util.List;

/** Coordinator for the NTP appearance theme collections bottom sheet in the NTP customization. */
@NullMarked
public class NtpThemeCollectionsCoordinator {
    // TODO(crbug.com/423579377): Update the url for learn more button.
    private static final String LEARN_MORE_CLICK_URL =
            "https://support.google.com/chrome/?p=new_tab";

    private final List<Pair<String, Integer>> mThemeCollectionsList = new ArrayList<>();
    private final BottomSheetDelegate mBottomSheetDelegate;
    private final Context mContext;
    private final View mNtpThemeCollectionsBottomSheetView;
    private final View mBackButton;
    private final ImageView mLearnMoreButton;
    private final RecyclerView mThemeCollectionsBottomSheetRecyclerView;
    private NtpThemeCollectionsAdapter mNtpThemeCollectionsAdapter;
    private @Nullable NtpSingleThemeCollectionCoordinator mNtpSingleThemeCollectionCoordinator;

    public NtpThemeCollectionsCoordinator(Context context, BottomSheetDelegate delegate) {
        mContext = context;
        mBottomSheetDelegate = delegate;

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

        // TODO(crbug.com/423579377): Generate this theme collections list.

        // Build the RecyclerView containing theme collections in the bottom sheet.
        mThemeCollectionsBottomSheetRecyclerView =
                mNtpThemeCollectionsBottomSheetView.findViewById(
                        R.id.theme_collections_recycler_view);
        mThemeCollectionsBottomSheetRecyclerView.setLayoutManager(
                new GridLayoutManager(context, /* spanCount= */ 2));
        mNtpThemeCollectionsAdapter =
                new NtpThemeCollectionsAdapter(
                        mThemeCollectionsList,
                        THEME_COLLECTIONS_ITEM,
                        this::handleThemeCollectionClick);
        mThemeCollectionsBottomSheetRecyclerView.setAdapter(mNtpThemeCollectionsAdapter);
    }

    public void destroy() {
        mBackButton.setOnClickListener(null);
        mLearnMoreButton.setOnClickListener(null);

        if (mNtpThemeCollectionsAdapter != null) {
            mNtpThemeCollectionsAdapter.clearOnClickListeners();
        }

        if (mNtpSingleThemeCollectionCoordinator != null) {
            mNtpSingleThemeCollectionCoordinator.destroy();
        }
    }

    void handleThemeCollectionClick(View view) {
        TextView titleView = view.findViewById(R.id.theme_collection_title);
        String themeCollectionTitle = "";
        if (titleView != null) {
            themeCollectionTitle = titleView.getText().toString();
        }

        if (mNtpSingleThemeCollectionCoordinator != null) {
            mNtpSingleThemeCollectionCoordinator.updateThemeCollection(themeCollectionTitle);
        } else {
            mNtpSingleThemeCollectionCoordinator =
                    new NtpSingleThemeCollectionCoordinator(
                            mContext, mBottomSheetDelegate, themeCollectionTitle);
        }

        mBottomSheetDelegate.showBottomSheet(BottomSheetType.SINGLE_THEME_COLLECTION);
    }

    void handleLearnMoreClick(View view) {
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

    @Nullable
            NtpSingleThemeCollectionCoordinator getNtpSingleThemeCollectionCoordinatorForTesting() {
        return mNtpSingleThemeCollectionCoordinator;
    }
}
