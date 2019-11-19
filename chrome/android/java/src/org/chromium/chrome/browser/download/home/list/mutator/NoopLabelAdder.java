// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of {@link LabelAdder} that doesn't insert any extra labels.
 */
public class NoopLabelAdder implements DateOrderedListMutator.LabelAdder {
    @Override
    public List<ListItem> addLabels(List<OfflineItem> sortedList) {
        List<ListItem> listItems = new ArrayList<>();
        for (OfflineItem offlineItem : sortedList) {
            listItems.add(new ListItem.OfflineItemListItem(offlineItem));
        }

        return listItems;
    }
}
