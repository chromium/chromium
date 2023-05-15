// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.browser_ui.widget.chips.ChipsCoordinator;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Displays a chip list containing tags for the current folder. */
public class PowerBookmarkTagChipList extends FrameLayout {
    private BookmarkId mCurrentFolder;
    private BookmarkModel mBookmarkModel;
    private ChipsCoordinator mChipsCoordinator;

    private final ModelList mChipList = new ModelList();
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

        mChipsCoordinator = new ChipsCoordinator(getContext(), mChipList);
        mChipsCoordinator.setSpaceItemDecoration(
                getResources().getDimensionPixelSize(
                        R.dimen.contextual_search_chip_list_chip_spacing),
                getResources().getDimensionPixelSize(
                        R.dimen.contextual_search_chip_list_side_padding));
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
        mChipList.clear();
        for (BookmarkId id : mBookmarkModel.getChildIds(mCurrentFolder)) {
            BookmarkItem item = mBookmarkModel.getBookmarkById(id);
            // TODO(crbug.com/1247825): Call #populateChipsForPowerBookmarkMeta will bookmark
            // metadata once available.
        }
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
        Collections.sort(
                entryList, (Map.Entry<String, Integer> o1, Map.Entry<String, Integer> o2) -> {
                    return o2.getValue().compareTo(o1.getValue());
                });

        for (int i = 0; i < entryList.size(); i++) {
            Map.Entry<String, Integer> entry = entryList.get(i);
            String tag = entry.getKey();

            ListItem chipListItem =
                    ChipsCoordinator.buildChipListItem(i, tag, this::toggleSelectionForTag);
            chipListItem.model.set(ChipProperties.ENABLED, true);
            mChipList.add(chipListItem);
        }
    }

    private void toggleSelectionForTag(PropertyModel chipModel) {
        if (chipModel == null) {
            assert false : "Attempt to select a tag that's not present!";
            return;
        }

        chipModel.set(ChipProperties.SELECTED, !chipModel.get(ChipProperties.SELECTED));
    }
}
