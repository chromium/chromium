// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.chips.Chip;
import org.chromium.components.browser_ui.widget.chips.ChipsCoordinator;
import org.chromium.components.browser_ui.widget.chips.ChipsProvider;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Displays a chip list containing tags for the current folder. */
public class PowerBookmarkTagChipList extends FrameLayout implements ChipsProvider {
    private BookmarkId mCurrentFolder;
    private BookmarkModel mBookmarkModel;
    private ChipsCoordinator mChipsCoordinator;

    private final ObserverList<ChipsProvider.Observer> mChipsProviderObservers =
            new ObserverList<>();
    private final List<Chip> mChips = new ArrayList<>();
    private final Map<String, Integer> mTagMap = new HashMap<>();

    private BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkModelChanged() {
            populateChipsForCurrentFolder();
        }
    };

    /**
     * Constructor for inflating from XML.
     */
    public PowerBookmarkTagChipList(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        mChipsCoordinator = new ChipsCoordinator(getContext(), /* chipsProvider= */ this);
        addView(mChipsCoordinator.getView());
    }

    /**
     * Initializes the object with the given {@link BookmarkModel} which allows observation of
     * changes the the underlying data.
     * @param bookmarkModel The {@link BookmarkModel} which manages bookmark data.
     */
    public void init(BookmarkModel bookmarkModel) {
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(mBookmarkModelObserver);
    }

    /**
     * Sets the current folder which contains the bookmarks to populate the tag chip list. The
     * tag chip list is currently populated under the assumption that all the relevant bookmarks
     * will be direct children of the given folder.
     * @param currentFolder The current folder to retrieve child bookmarks from.
     */
    public void setBookmarkFolder(BookmarkId currentFolder) {
        mCurrentFolder = currentFolder;
        populateChipsForCurrentFolder();
    }

    private void populateChipsForCurrentFolder() {
        mTagMap.clear();
        mChips.clear();
        for (BookmarkId id : mBookmarkModel.getChildIDs(mCurrentFolder)) {
            BookmarkItem item = mBookmarkModel.getBookmarkById(id);
            // TODO(crbug.com/1247825): Call #populateChipsForPowerBookmarkMeta will bookmark
            // metadata once available.
        }
        notifyObservers();
    }

    @VisibleForTesting
    void populateTagMapForPowerBookmarkMeta(PowerBookmarkMeta powerBookmarkMeta) {
        for (int i = 0; i < powerBookmarkMeta.getTagsCount(); i++) {
            String tag = powerBookmarkMeta.getTags(i).getDisplayName();
            if (!mTagMap.containsKey(tag)) mTagMap.put(tag, 0);

            mTagMap.put(tag, mTagMap.get(tag) + 1);
        }
    }

    @VisibleForTesting
    void populateChipListFromCurrentTagMap() {
        List<Map.Entry<String, Integer>> entryList = new ArrayList<>(mTagMap.entrySet());
        Collections.sort(entryList, new Comparator<Map.Entry<String, Integer>>() {
            @Override
            public int compare(Map.Entry<String, Integer> o1, Map.Entry<String, Integer> o2) {
                return o2.getValue().compareTo(o1.getValue());
            }
        });

        for (int i = 0; i < entryList.size(); i++) {
            Map.Entry<String, Integer> entry = entryList.get(i);
            String tag = entry.getKey();

            Chip chip =
                    new Chip(i, tag, Chip.INVALID_ICON_ID, () -> { toggleSelectionForTag(tag); });
            chip.enabled = true;
            mChips.add(chip);
        }
    }

    private void toggleSelectionForTag(String tag) {
        Chip chip = getChipForTag(tag);
        if (chip == null) {
            assert false : "Attempt to select a tag that's not present, tag=" + tag;
            return;
        }

        chip.selected = !chip.selected;
        notifyObservers();
    }

    private Chip getChipForTag(String tag) {
        for (int i = 0; i < mChips.size(); i++) {
            Chip chip = mChips.get(i);
            if (TextUtils.equals(chip.rawText, tag)) return chip;
        }
        return null;
    }

    @VisibleForTesting
    void notifyObservers() {
        for (Observer observer : mChipsProviderObservers) {
            observer.onChipsChanged();
        }
    }

    // ChipsProvider implementation

    @Override
    public void addObserver(Observer observer) {
        mChipsProviderObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mChipsProviderObservers.removeObserver(observer);
    }

    @Override
    public List<Chip> getChips() {
        return mChips;
    }

    @Override
    public int getChipSpacingPx() {
        return getContext().getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.contextual_search_chip_list_chip_spacing);
    }

    @Override
    public int getSidePaddingPx() {
        return getContext().getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.contextual_search_chip_list_side_padding);
    }
}