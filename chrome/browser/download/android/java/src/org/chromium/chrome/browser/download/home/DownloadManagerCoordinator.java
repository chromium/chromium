// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.view.View;

import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * A coordinator that represents the main download manager UI page. This visually shows a list of
 * downloaded items and allows the user to interact with those items.
 */
public interface DownloadManagerCoordinator {
    /**
     * An observer to be notified of internal state changes that should be represented as a URL
     * change externally.
     */
    public interface Observer {
        /** Called when the url representing the internal state of the coordinator has changed. */
        void onUrlChanged(String url);
    }

    /** To be called when the coordinator should be destroyed and is no longer in use. */
    void destroy();

    /**
     * Handlers interested in intercepting back events. The first handler has the top priority
     * and the last one has the least.
     * @return  Handlers which are interested in consuming back press event.
     */
    BackPressHandler[] getBackPressHandlers();

    /** @return A {@link View} representing this coordinator. */
    View getView();

    /** To be called to push the url containing internal state to the coordinator. */
    void updateForUrl(String url);

    /** Adds {@code observer} to be notified of url state changes. */
    void addObserver(Observer observer);

    /** Stops notifying {@code observer} of url state changes. */
    void removeObserver(Observer observer);
}
