// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.client.lifecycle;

/**
 * Interface to allow host applications to communicate changes in state to Feed.
 *
 * <p>Note that these are related to app lifecycle, not UI lifecycle
 */
public interface AppLifecycleListener {
    /** Called after critical loading has completed but before Feed is rendered. */
    void onEnterForeground();

    /** Called when the app is backgrounded, will perform clean up. */
    void onEnterBackground();

    /**
     * Called when host wants to clear all data. Will delete content without changing opt-in /
     * opt-out status.
     */
    void onClearAll();

    /** Called to clear all data then initiate a refresh. */
    void onClearAllWithRefresh();

    /** Called when user signs in. */
    void onSignedIn();

    /** Called when user signs out. */
    void onSignedOut();

    /**
     * Called when the host wants the Feed to perform any heavyweight initialization it might need
     * to do. This is the only trigger for the initialization process; if it’s not called, the host
     * should not expect the Feed to be able to render cards.
     */
    void initialize();
}
