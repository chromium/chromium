// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

/**
 * The interface of most visited tiles layout.
 */
public interface MostVisitedTilesLayout {
    /**
     * @param isNtpAsHomeSurfaceEnabled {@code true} if showing an NTP as the home
     *                                  surface in the given context.
     */
    void setIsNtpAsHomeSurfaceEnabled(boolean isNtpAsHomeSurfaceEnabled);
}
