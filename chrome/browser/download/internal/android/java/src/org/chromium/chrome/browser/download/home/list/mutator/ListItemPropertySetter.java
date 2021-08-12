// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;

import java.util.List;

/**
 * Post processes the items in the list and sets properties for UI as appropriate. The properties
 * being set are:
 * - Image item span width.
 */
public class ListItemPropertySetter implements ListConsumer {
    private final DownloadManagerUiConfig mConfig;
    private ListConsumer mListConsumer;

    /** Constructor. */
    public ListItemPropertySetter(DownloadManagerUiConfig config) {
        mConfig = config;
    }

    @Override
    public void onListUpdated(List<ListItem> inputList) {
        setProperties(inputList);
        mListConsumer.onListUpdated(inputList);
    }

    @Override
    public ListConsumer setListConsumer(ListConsumer nextConsumer) {
        mListConsumer = nextConsumer;
        return mListConsumer;
    }

    /** Sets properties for items in the given list. */
    private void setProperties(List<ListItem> sortedList) {
        setWidthForImageItems(sortedList);
    }

    private void setWidthForImageItems(List<ListItem> listItems) {
        if (!mConfig.supportFullWidthImages) return;

        for (int i = 0; i < listItems.size(); i++) {
            ListItem currentItem = listItems.get(i);
            boolean currentItemIsImage = currentItem instanceof OfflineItemListItem
                    && ((OfflineItemListItem) currentItem).item.filter == OfflineItemFilter.IMAGE;
            if (!currentItemIsImage) continue;

            ListItem previousItem = i == 0 ? null : listItems.get(i - 1);
            ListItem nextItem = i >= listItems.size() - 1 ? null : listItems.get(i + 1);
            boolean previousItemIsImage = previousItem instanceof OfflineItemListItem
                    && ((OfflineItemListItem) previousItem).item.filter == OfflineItemFilter.IMAGE;
            boolean nextItemIsImage = nextItem instanceof OfflineItemListItem
                    && ((OfflineItemListItem) nextItem).item.filter == OfflineItemFilter.IMAGE;

            if (!previousItemIsImage && !nextItemIsImage) {
                ((OfflineItemListItem) currentItem).spanFullWidth = true;
            }
        }
    }
}
