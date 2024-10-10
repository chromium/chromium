// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

/**
 * A factory that creates a concrete subclass of {@link TabGroupModelFilterBase} for {@link
 * TabModel}s.
 */
public interface TabGroupModelFilterFactory {
    /**
     * @param model The {@link TabModel} to serve as base for the new filter.
     * @return A new {@link TabGroupModelFilterBase} for {@code model}.
     */
    TabGroupModelFilterBase createTabGroupModelFilter(TabModel model);
}
