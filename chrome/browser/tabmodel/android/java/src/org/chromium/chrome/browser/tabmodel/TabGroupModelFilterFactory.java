// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;

/**
 * A factory that creates a concrete subclass of {@link TabGroupModelFilterInternal} for {@link
 * TabModel}s.
 */
interface TabGroupModelFilterFactory {
    /**
     * @param model The {@link TabModel} to serve as base for the new filter.
     * @param tabUngrouper The {@link TabUngrouper} to use for the filter.
     * @return A new {@link TabGroupModelFilterBase} for {@code model}.
     */
    /*package*/ @NonNull
    TabGroupModelFilterInternal createTabGroupModelFilter(
            @NonNull TabModel model, @NonNull TabUngrouper tabUngrouper);
}
