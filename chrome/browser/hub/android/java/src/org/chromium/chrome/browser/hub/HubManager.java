// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;

/**
 * This is the primary interface for interacting with the Hub. Create using {@link
 * HubManagerFactory}.
 */
public interface HubManager {
    /** Destroys the {@link HubManager}, it cannot be used again. */
    public void destroy();

    /** Returns the {@link PaneManager} for interacting with {@link Pane}s. */
    public @NonNull PaneManager getPaneManager();

    /**
     * Returns the {@link HubController} used by the {@link HubLayout} to control the visibility and
     * display of the Hub.
     */
    public @NonNull HubController getHubController();
}
