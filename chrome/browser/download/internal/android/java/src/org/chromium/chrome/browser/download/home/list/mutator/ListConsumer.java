// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.list.ListItem;

import java.util.List;

/**
 * A generic interface to be notified when a list of {@link ListItem} has changed. The consumer
 * processes the list, modifies it, and passes it or a new list to the next consumer in the
 * chain.
 */
public interface ListConsumer {
    /**
     * Called to notify that the underlying list has changed.
     * @param inputList The updated input list.
     */
    void onListUpdated(List<ListItem> inputList);

    /**
     * Sets the downstream {@link ListConsumer} that should be notified of changes in this {@link
     * ListConsumer}.
     * @param nextConsumer The next {@link ListConsumer} in the chain.
     * @return The next {@link ListConsumer} in the chain if any.
     */
    ListConsumer setListConsumer(ListConsumer nextConsumer);
}
