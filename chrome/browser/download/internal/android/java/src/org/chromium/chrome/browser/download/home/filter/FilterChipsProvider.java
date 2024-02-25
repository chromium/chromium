// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.content.Context;
import android.os.Handler;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.browser_ui.widget.chips.ChipsCoordinator;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

/**
 * A class that provides a subset of {@link Filters} to be used as a chip selector for filtering
 * downloads.
 */
public class FilterChipsProvider implements OfflineItemFilterObserver {
    /** A delegate responsible for handling UI actions like selecting filters. */
    public interface Delegate {
        /** Called when the selected filter has changed. */
        void onFilterSelected(@FilterType int filterType);
    }

    private final Context mContext;
    private final Delegate mDelegate;
    private final OfflineItemFilterSource mSource;

    private final Handler mHandler = new Handler();
    private final ModelList mSortedChips = new ModelList();
    private final ModelList mVisibleChips = new ModelList();

    /** Builds a new FilterChipsBackend. */
    public FilterChipsProvider(Context context, Delegate delegate, OfflineItemFilterSource source) {
        mContext = context;
        mDelegate = delegate;
        mSource = source;

        Callback<PropertyModel> chipSelectedCallback =
                (model) -> onChipSelected(model.get(ChipProperties.ID));

        ListItem noneChip =
                ChipsCoordinator.buildChipListItem(
                        Filters.FilterType.NONE,
                        context.getString(R.string.download_manager_ui_all_downloads),
                        chipSelectedCallback,
                        R.drawable.settings_all_sites);

        ListItem videosChip =
                ChipsCoordinator.buildChipListItem(
                        Filters.FilterType.VIDEOS,
                        context.getString(R.string.download_manager_ui_video),
                        chipSelectedCallback,
                        R.drawable.ic_videocam_24dp);

        ListItem musicChip =
                ChipsCoordinator.buildChipListItem(
                        Filters.FilterType.MUSIC,
                        context.getString(R.string.download_manager_ui_audio),
                        chipSelectedCallback,
                        R.drawable.ic_music_note_24dp);

        ListItem imagesChip =
                ChipsCoordinator.buildChipListItem(
                        Filters.FilterType.IMAGES,
                        context.getString(R.string.download_manager_ui_images),
                        chipSelectedCallback,
                        R.drawable.ic_drive_image_24dp);

        ListItem sitesChip =
                ChipsCoordinator.buildChipListItem(
                        Filters.FilterType.SITES,
                        context.getString(R.string.download_manager_ui_pages),
                        chipSelectedCallback,
                        R.drawable.ic_globe_24dp);

        ListItem otherChip =
                ChipsCoordinator.buildChipListItem(
                        Filters.FilterType.OTHER,
                        context.getString(R.string.download_manager_ui_other),
                        chipSelectedCallback,
                        R.drawable.ic_drive_file_24dp);

        // By default select the none chip.
        noneChip.model.set(ChipProperties.SELECTED, true);

        mSortedChips.add(noneChip);
        mSortedChips.add(videosChip);
        mSortedChips.add(musicChip);
        mSortedChips.add(imagesChip);
        mSortedChips.add(sitesChip);
        mSortedChips.add(otherChip);

        mSource.addObserver(this);
        generateFilterStates();
    }

    /**
     * Sets the filter that is currently selected.
     * @param type The type of filter to select.
     */
    public void setFilterSelected(@FilterType int type) {
        for (int i = 0; i < mSortedChips.size(); i++) {
            PropertyModel chip = mSortedChips.get(i).model;
            chip.set(ChipProperties.SELECTED, chip.get(ChipProperties.ID) == type);
        }
    }

    /**
     * @return The {@link FilterType} of the selected filter or {@link Filters#NONE} if no filter
     * is selected.
     */
    public @FilterType int getSelectedFilter() {
        for (ListItem chip : mSortedChips) {
            if (chip.model.get(ChipProperties.SELECTED)) return chip.model.get(ChipProperties.ID);
        }

        return Filters.FilterType.NONE;
    }

    public ModelList getChips() {
        return mVisibleChips;
    }

    private void updateVisibleChips() {
        mVisibleChips.clear();
        for (ListItem chip : mSortedChips) {
            if (chip.model.get(ChipProperties.ENABLED)) mVisibleChips.add(chip);
        }

        // If there is only one chip type or only NONE chips, remove the entire row of chips.
        if (mVisibleChips.size() <= 2) {
            assert mVisibleChips.get(0).model.get(ChipProperties.ID) == FilterType.NONE;
            mVisibleChips.clear();
        }
    }

    // OfflineItemFilterObserver implementation.
    // Post calls to generateFilterStates() avoid re-entrancy.
    @Override
    public void onItemsAdded(Collection<OfflineItem> items) {
        mHandler.post(() -> generateFilterStates());
    }

    @Override
    public void onItemsRemoved(Collection<OfflineItem> items) {
        mHandler.post(() -> generateFilterStates());
    }

    @Override
    public void onItemUpdated(OfflineItem oldItem, OfflineItem item) {
        if (oldItem.filter == item.filter) return;
        mHandler.post(() -> generateFilterStates());
    }

    private void generateFilterStates() {
        // Build a map of all filter types in our data set.
        Map</* @FilterType */ Integer, /*item count*/ Integer> filters = new HashMap<>();
        for (OfflineItem item : mSource.getItems()) {
            int filter = Filters.fromOfflineItem(item);
            int itemCount = (filters.containsKey(filter) ? filters.get(filter) : 0) + 1;
            filters.put(filter, itemCount);
        }

        int noneChipItemCount = 0;
        for (Map.Entry<Integer, Integer> entry : filters.entrySet()) {
            if (entry.getKey() != FilterType.PREFETCHED) noneChipItemCount += entry.getValue();
        }

        filters.put(Filters.FilterType.NONE, noneChipItemCount);

        // Set the enabled states correctly for all chips.
        for (ListItem chip : mSortedChips) {
            int chipId = chip.model.get(ChipProperties.ID);
            boolean shouldEnable = filters.containsKey(chipId);
            chip.model.set(ChipProperties.ENABLED, shouldEnable);
            if (chip.model.get(ChipProperties.ENABLED)) {
                chip.model.set(
                        ChipProperties.CONTENT_DESCRIPTION,
                        UiUtils.getChipContentDescription(
                                mContext.getResources(), chipId, filters.get(chipId)));
            }
        }
        updateVisibleChips();

        // Validate that selection is on a valid type.
        for (ListItem chip : mSortedChips) {
            if (chip.model.get(ChipProperties.SELECTED)
                    && !chip.model.get(ChipProperties.ENABLED)) {
                onChipSelected(Filters.FilterType.NONE);
                break;
            }
        }
    }

    private void onChipSelected(@FilterType int id) {
        setFilterSelected(id);
        mDelegate.onFilterSelected(id);
    }
}
