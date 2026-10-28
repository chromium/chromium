// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.prefs.PrefService;

/** Prints tabs. Only available when printing is part of the build. */
@NullMarked
public interface PrintingDelegate {
    /**
     * Returns the printing implementation, or {@code null} when printing is not built. Features
     * that print should have the delegate provided by their embedder rather than calling this.
     */
    static @Nullable PrintingDelegate getInstance() {
        return ServiceLoaderUtil.maybeCreate(PrintingDelegate.class);
    }

    /** Returns whether printing is enabled by {@code prefs}. */
    boolean isPrintingEnabled(PrefService prefs);

    /** Starts printing {@code tab}, returning whether printing was started. */
    boolean print(@Nullable Tab tab);
}
