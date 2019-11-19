// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.content.Context;
import android.os.Handler;

import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.filter.chips.Chip;
import org.chromium.chrome.browser.download.home.filter.chips.ChipsProvider;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A {@link ChipsProvider} implementation that wraps a subset of {@link Filters} to be used as a
 * chip selector for filtering downloads.
 */
public class FilterChipsProvider implements ChipsProvider, OfflineItemFilterObserver {
    private static final int INVALID_INDEX = -1;

    /** A delegate responsible for handling UI actions like selecting filters. */
    public interface Delegate {
        /** Called when the selected filter has changed. */
        void onFilterSelected(@FilterType int filterType);
    }

    private final Context mContext;
    private final Delegate mDelegate;
    private final OfflineItemFilterSource mSource;

    private final Handler mHandler = new Handler();
    private final ObserverList<Observer> mObservers = new ObserverList<ChipsProvider.Observer>();
    private final List<Chip> mSortedChips = new ArrayList<>();

    /** Builds a new FilterChipsBackend. */
    public FilterChipsProvider(Context context, Delegate delegate, OfflineItemFilterSource source) {
        mContext = context;
        mDelegate = delegate;
        mSource = source;

        Chip noneChip = new Chip(Filters.FilterType.NONE,
                R.string.download_manager_ui_all_downloads, R.drawable.settings_all_sites,
                () -> onChipSelected(Filters.FilterType.NONE));
        Chip videosChip = new Chip(Filters.FilterType.VIDEOS, R.string.download_manager_ui_video,
                R.drawable.ic_videocam_24dp, () -> onChipSelected(Filters.FilterType.VIDEOS));
        Chip musicChip = new Chip(Filters.FilterType.MUSIC, R.string.download_manager_ui_audio,
                R.drawable.ic_music_note_24dp, () -> onChipSelected(Filters.FilterType.MUSIC));
        Chip imagesChip = new Chip(Filters.FilterType.IMAGES, R.string.download_manager_ui_images,
                R.drawable.ic_drive_image_24dp, () -> onChipSelected(Filters.FilterType.IMAGES));
        Chip sitesChip = new Chip(Filters.FilterType.SITES, R.string.download_manager_ui_pages,
                R.drawable.ic_globe_24dp, () -> onChipSelected(Filters.FilterType.SITES));
        Chip otherChip = new Chip(Filters.FilterType.OTHER, R.string.download_manager_ui_other,
                R.drawable.ic_drive_file_24dp, () -> onChipSelected(Filters.FilterType.OTHER));

        // By default select the none chip.
        noneChip.selected = true;

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
            Chip chip = mSortedChips.get(i);
            boolean willSelect = chip.id == type;

            // Early out if we're already selecting the appropriate Chip type.
            if (chip.selected && willSelect) return;
            if (chip.selected == willSelect) continue;
            chip.selected = willSelect;
        }

        for (Observer observer : mObservers) observer.onChipsChanged();
    }

    /**
     * @return The {@link FilterType} of the selected filter or {@link Filters#NONE} if no filter
     * is selected.
     */
    public @FilterType int getSelectedFilter() {
        for (Chip chip : mSortedChips) {
            if (chip.selected) return chip.id;
        }

        return Filters.FilterType.NONE;
    }

    // ChipsProvider implementation.
    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public List<Chip> getChips() {
        List<Chip> visibleChips = new ArrayList<>();
        for (Chip chip : mSortedChips) {
            if (chip.enabled) visibleChips.add(chip);
        }

        // If there is only one chip type or only NONE chips, remove the entire row of chips.
        if (visibleChips.size() <= 2) {
            assert visibleChips.get(0).id == FilterType.NONE;
            visibleChips.clear();
        }

        return visibleChips;
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
        boolean chipsHaveChanged = false;
        for (Chip chip : mSortedChips) {
            boolean shouldEnable = filters.containsKey(chip.id);
            chipsHaveChanged |= (shouldEnable != chip.enabled);
            chip.enabled = shouldEnable;
            if (chip.enabled) {
                chip.contentDescription = UiUtils.getChipContentDescription(
                        mContext.getResources(), chip.id, filters.get(chip.id));
            }
        }

        if (chipsHaveChanged) {
            for (Observer observer : mObservers) observer.onChipsChanged();
        }

        // Validate that selection is on a valid type.
        for (Chip chip : mSortedChips) {
            if (chip.selected && !chip.enabled) {
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