// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;

/**
 * This is the primary interface for interacting with the Hub. Create using {@link
 * HubManagerFactory}.
 */
public interface HubManager {
    /** Destroys the {@link HubManager}, it cannot be used again. */
    void destroy();

    /** Returns the {@link PaneManager} for interacting with {@link Pane}s. */
    @NonNull
    PaneManager getPaneManager();

    /**
     * Returns the {@link HubController} used by the {@link HubLayout} to control the visibility and
     * display of the Hub.
     */
    @NonNull
    HubController getHubController();

    /** Returns a supplier that contains true when the Hub is visible and false otherwise. */
    @NonNull
    ObservableSupplier<Boolean> getHubVisibilitySupplier();

    /**
     * Returns the {@link HubShowPaneHelper} used to select a pane before opening the {@link
     * HubLayout}.
     */
    @NonNull
    HubShowPaneHelper getHubShowPaneHelper();

    /** Sets the status indicator height. */
    void setStatusIndicatorHeight(int height);

    /** Sets the app header height. */
    void setAppHeaderHeight(int height);
}
