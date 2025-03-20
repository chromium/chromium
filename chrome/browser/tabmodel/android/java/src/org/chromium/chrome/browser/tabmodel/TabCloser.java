// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;

/** Interface denoting a class that closes tabs. */
@NullMarked
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public interface TabCloser {
    /**
     * Closes tabs based on the provided parameters. Refer to {@link TabClosureParams} for different
     * ways to close tabs. The public API for this is {@link TabRemover}.
     *
     * @param tabClosureParams The parameters to follow when closing tabs.
     * @return Whether the tab closure succeeded (only possibly false for single tab closure).
     */
    boolean closeTabs(TabClosureParams tabClosureParams);
}
