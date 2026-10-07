// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/** Observer interface for data change events emitted by {@link TabListDataProvider}. */
@NullMarked
public interface TabListDataObserver {
    /**
     * Called when the entire data set is reset.
     *
     * @param items The new full list of {@link TabListItem} entries.
     */
    default void onDataReset(List<? extends TabListItem> items) {}

    // TODO(crbug.com/562590772): Add incremental structural and property update callbacks.
}
