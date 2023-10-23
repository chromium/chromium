// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;

/**
 * An observer that listens for active {@link Layout} changes.
 *
 * DEPRECATED, please use {@link LayoutStateObserver} instead.
 */
@Deprecated
public interface SceneChangeObserver {
    /**
     * Called when the active {@link Layout} changes.
     * @param layout The new active {@link Layout}.
     *
     * DEPRECATED, please use {@link LayoutStateObserver#onStartedShowing(int)} instead.
     */
    void onSceneChange(Layout layout);
}
