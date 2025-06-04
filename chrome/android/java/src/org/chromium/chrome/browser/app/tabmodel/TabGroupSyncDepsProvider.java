// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncDelegate;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncDelegate.Deps;

/**
 * Provider for providing chrome layer dependencies for constructing the delegate of {@link
 * TabGroupSyncService}.
 */
@NullMarked
public class TabGroupSyncDepsProvider {
    /** Constructor. */
    @CalledByNative
    private static TabGroupSyncDelegate.Deps createDeps() {
        return new Deps(TabWindowManagerSingleton.getInstance());
    }
}
