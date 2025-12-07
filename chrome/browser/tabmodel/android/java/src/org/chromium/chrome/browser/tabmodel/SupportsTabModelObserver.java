// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;

/** Interface to support adding and removing {@link TabModelObserver}s. */
@NullMarked
public interface SupportsTabModelObserver {
    /**
     * Subscribes a {@link TabModelObserver} to be notified about changes to a {@link TabModel}.
     *
     * @param observer The observer to be subscribed.
     */
    void addObserver(TabModelObserver observer);

    /**
     * Unsubscribes a previously subscribed {@link TabModelObserver}.
     *
     * @param observer The observer to be unsubscribed.
     */
    void removeObserver(TabModelObserver observer);
}
