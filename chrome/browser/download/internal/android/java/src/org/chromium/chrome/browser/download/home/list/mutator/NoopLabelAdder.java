// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.list.ListItem;

import java.util.List;

/** Implementation of {@link LabelAdder} that doesn't add any labels. */
public class NoopLabelAdder implements ListConsumer {
    private ListConsumer mListConsumer;

    public NoopLabelAdder() {}

    @Override
    public ListConsumer setListConsumer(ListConsumer consumer) {
        mListConsumer = consumer;
        return mListConsumer;
    }

    @Override
    public void onListUpdated(List<ListItem> inputList) {
        if (mListConsumer == null) return;
        mListConsumer.onListUpdated(addLabels(inputList));
    }

    private List<ListItem> addLabels(List<ListItem> sortedList) {
        return sortedList;
    }
}
