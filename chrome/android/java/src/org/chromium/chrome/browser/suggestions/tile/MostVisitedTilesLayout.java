// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

/**
 * The interface of most visited tiles layout.
 */
public interface MostVisitedTilesLayout {
    /**
     * @param isMultiColumnFeedOnTabletEnabled {@code true} if both showing an NTP as the home
     *                                         surface and multiple column Feed are enabled in the
     *                                         given context.
     */
    void setIsMultiColumnFeedOnTabletEnabled(boolean isMultiColumnFeedOnTabletEnabled);
}
