// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.PlatformType;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataCustomizedColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataGroup;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** Coordinator for the NTP theme sync history. */
@NullMarked
public class NtpThemeSyncHistoryCoordinator {
    private final Context mContext;
    private final BottomSheetDelegate mBottomSheetDelegate;
    private final PropertyModel mPropertyModel;
    private final NtpBackgroundDataManager mNtpBackgroundDataManager;
    private final List<NtpBackgroundDataBase> mDataShowingList;
    private final NtpBackgroundDataColor mDefaultNtpBackgroundData;

    private @Nullable NtpThemeSyncHistoryRecyclerViewAdaptor mRecyclerViewAdaptor;
    private @Nullable NtpBackgroundDataBase mLastSelectedNtpBackgroundData;
    private int mLastSelectedIndex;

    public NtpThemeSyncHistoryCoordinator(
            Context context,
            ViewGroup parentView,
            BottomSheetDelegate bottomSheetDelegate,
            View.OnClickListener moreOptionsClickListener) {
        mContext = context;
        mBottomSheetDelegate = bottomSheetDelegate;

        ViewGroup historyContainerView =
                parentView.findViewById(R.id.ntp_theme_sync_history_container);
        mPropertyModel = new PropertyModel(NtpThemeSyncHistoryProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                mPropertyModel, historyContainerView, NtpThemeSyncHistoryContainerViewBinder::bind);

        mNtpBackgroundDataManager = new NtpBackgroundDataManager(mContext);
        mPropertyModel.set(NtpThemeSyncHistoryProperties.IS_VISIBLE, true);
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(mContext, LinearLayoutManager.HORIZONTAL, false);
        mPropertyModel.set(
                NtpThemeSyncHistoryProperties.RECYCLER_VIEW_LAYOUT_MANAGER, layoutManager);
        mPropertyModel.set(
                NtpThemeSyncHistoryProperties.MORE_OPTIONS_CLICK_LISTENER,
                moreOptionsClickListener);

        mDefaultNtpBackgroundData =
                new NtpBackgroundDataColor(
                        context, PlatformType.ANDROID_LOCAL, NtpThemeColorId.DEFAULT, false);
        mDataShowingList = new ArrayList<>();
    }

    /** Prepare data before showing the NTP theme history. */
    int prepareData() {
        // The default option is placed at the first.
        mDataShowingList.add(mDefaultNtpBackgroundData);
        int lastSelectedIndex = 0;

        // Adds all history data to the list.
        NtpBackgroundDataGroup[] groups =
                mNtpBackgroundDataManager.getBackgroundDataListFromSharedPreference();
        NtpBackgroundDataGroup localGroup = groups[PlatformType.ANDROID_LOCAL];
        assumeNonNull(localGroup);
        if (!localGroup.isEmpty()) {
            // The last selected index is set to the first item from local selected history if it
            // isn't empty.
            lastSelectedIndex = 1;
            mDataShowingList.addAll(localGroup.getList());
        }

        // Adds sync data from remote platforms.
        for (int i = PlatformType.ANDROID_REMOTE; i < PlatformType.MAX_COUNT; i++) {
            NtpBackgroundDataGroup remoteDataGroup = groups[i];
            if (remoteDataGroup == null || remoteDataGroup.isEmpty()) continue;

            // Finds the first remote data which isn't in the local history.
            for (NtpBackgroundDataBase data : remoteDataGroup) {
                // Checks if the current remote data exists in the local history.
                int index = localGroup.indexOf(data);
                if (index == -1) {
                    // Adds the data and stops here.
                    mDataShowingList.add(data);
                    break;
                }
            }
        }
        return lastSelectedIndex;
    }

    /** Called before showing the NTP theme customization history items. */
    public void prepareToShow() {
        mLastSelectedIndex = prepareData();
        mLastSelectedNtpBackgroundData = mDataShowingList.get(mLastSelectedIndex);

        mRecyclerViewAdaptor =
                new NtpThemeSyncHistoryRecyclerViewAdaptor(
                        mContext, mDataShowingList, this::onItemClicked, mLastSelectedIndex);
        mPropertyModel.set(
                NtpThemeSyncHistoryProperties.RECYCLER_VIEW_ADAPTER, mRecyclerViewAdaptor);
        // Sets the highlighted color item if user has chosen a customized color theme.
        mPropertyModel.set(
                NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX, mLastSelectedIndex);
    }

    private void onItemClicked(NtpBackgroundDataBase backgroundData) {
        boolean shouldRecreate = shouldRecreateActivity(backgroundData);
        mBottomSheetDelegate.onNewColorSelected(shouldRecreate);

        NtpCustomizationConfigManager.getInstance()
                .onBackgroundDataChanged(mContext, backgroundData);
        mLastSelectedNtpBackgroundData = backgroundData;
    }

    /** Returns whether to recreate the activity to apply the new theme color. */
    private boolean shouldRecreateActivity(NtpBackgroundDataBase backgroundData) {
        boolean isDifferentColor = true;
        if (isColorTheme(backgroundData)) {
            // We check the following color theme cases to see if color matches.
            if (backgroundData instanceof NtpBackgroundDataColor ntpBackgroundDataColor
                    && mLastSelectedNtpBackgroundData
                            instanceof NtpBackgroundDataColor lastSelectedBackgroundDataColor) {
                isDifferentColor =
                        !lastSelectedBackgroundDataColor
                                .getNtpThemeColorInfo()
                                .equals(ntpBackgroundDataColor.getNtpThemeColorInfo());
            } else if (backgroundData instanceof NtpBackgroundDataCustomizedColor customizedColor
                    && mLastSelectedNtpBackgroundData
                            instanceof
                            NtpBackgroundDataCustomizedColor lastSelectedCustomizedColor) {
                isDifferentColor =
                        !customizedColor
                                .getNtpThemeColorFromHexInfo()
                                .equals(lastSelectedCustomizedColor.getNtpThemeColorFromHexInfo());
            }
        }
        return isDifferentColor;
    }

    /** Called to destroy the NtpThemeSyncHistoryCoordinator. */
    public void destroy() {
        mDataShowingList.clear();
        mLastSelectedNtpBackgroundData = null;
        mPropertyModel.set(NtpThemeSyncHistoryProperties.MORE_OPTIONS_CLICK_LISTENER, null);
        mPropertyModel.set(NtpThemeSyncHistoryProperties.RECYCLER_VIEW_LAYOUT_MANAGER, null);
    }

    private boolean isColorTheme(NtpBackgroundDataBase backgroundData) {
        return backgroundData instanceof NtpBackgroundDataColor
                || backgroundData instanceof NtpBackgroundDataCustomizedColor;
    }

    PropertyModel getPropertyModelForTesting() {
        return mPropertyModel;
    }

    List<NtpBackgroundDataBase> getDataShowingListForTesting() {
        return mDataShowingList;
    }

    @Nullable NtpThemeSyncHistoryRecyclerViewAdaptor getRecyclerViewAdaptorForTesting() {
        return mRecyclerViewAdaptor;
    }
}
