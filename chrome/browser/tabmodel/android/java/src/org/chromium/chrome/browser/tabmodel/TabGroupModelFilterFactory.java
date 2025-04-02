// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;

/**
 * A factory that creates a concrete subclass of {@link TabGroupModelFilterInternal} for {@link
 * TabModel}s.
 */
@NullMarked
interface TabGroupModelFilterFactory {
    /**
     * @param model The {@link TabModelInternal} to serve as base for the new filter.
     * @param tabUngrouper The {@link TabUngrouper} to use for the filter.
     * @return A new {@link TabGroupModelFilterBase} for {@code model}.
     */
    /*package*/
    TabGroupModelFilterInternal createTabGroupModelFilter(
            TabModelInternal model, TabUngrouper tabUngrouper);
}
