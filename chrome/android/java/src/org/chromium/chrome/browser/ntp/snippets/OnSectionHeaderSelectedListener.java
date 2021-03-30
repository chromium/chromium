// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

/** Callback for when a section header is selected. */
public interface OnSectionHeaderSelectedListener {
    /**
     * Callback for when a header tab is selected.
     * @param index the index of the tab selected.
     */
    void onSectionHeaderSelected(int index);
}
