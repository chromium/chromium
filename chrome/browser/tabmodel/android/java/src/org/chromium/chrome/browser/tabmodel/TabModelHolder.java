// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;

/** Holds regular versions of {@link TabModelInternal} and {@link TabGroupModelFilterInternal}. */
@NullMarked
/*package*/ class TabModelHolder {
    public final TabModelInternal tabModel;
    public final TabGroupModelFilterInternal tabGroupModelFilter;

    public TabModelHolder(
            TabModelInternal tabModel, TabGroupModelFilterInternal tabGroupModelFilter) {
        this.tabModel = tabModel;
        this.tabGroupModelFilter = tabGroupModelFilter;
    }
}
