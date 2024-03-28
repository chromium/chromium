// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

/** A structure for holding the a recycler view position and offset. */
public class RecyclerViewPosition {
    private int mPosition;
    private int mOffset;

    /**
     * @param position The position of the first visible item in the recyclerView.
     * @param offset The scroll offset of the recyclerView;
     */
    public RecyclerViewPosition(int position, int offset) {
        mPosition = position;
        mOffset = offset;
    }

    /**
     * @return the position of the first visible item in the RecyclerView.
     */
    public int getPosition() {
        return mPosition;
    }

    /**
     * @return the offset from the first item in the RecyclerView.
     */
    public int getOffset() {
        return mOffset;
    }
}
