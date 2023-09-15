// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.common;

/**
 * Data identifying an item's position in a list. Used to create the a11y content description of
 * the items representing user's content, like credentials, and distinguish them from static
 * controls like the "Manage password" button.
 */
public class FillableItemCollectionInfo {
    private final int mPosition;
    private final int mTotal;

    /**
     * @param position 1-based position of the item in the list.
     * @param total Total number of items in the list.
     */
    public FillableItemCollectionInfo(int position, int total) {
        assert position <= total;

        mPosition = position;
        mTotal = total;
    }

    public int getPosition() {
        return mPosition;
    }

    public int getTotal() {
        return mTotal;
    }
}
