// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

/** Callback for when a section header is selected. */
public interface OnSectionHeaderSelectedListener {
    /**
     * Callback for when a header tab is selected.
     * @param index the index of the tab selected.
     */
    void onSectionHeaderSelected(int index);

    /**
     * Callback for when a header tab is reselected.
     *
     * This is called when the user clicks on a tab that's already selected.
     * {@link #onSectionHeaderSelected(int)} will not accompany this call.
     *
     * @param index the index of the tab that's reselected.
     */
    default void onSectionHeaderReselected(int index) {}

    /**
     * Callback for when a header tab is unselected.
     *
     * Unselected usually accompanies an {@link #onSectionHeaderSelected(int)}
     * for the new tab.
     *
     * @param index the index of the tab that's unselected.
     */
    default void onSectionHeaderUnselected(int index) {}
}
