// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

/** A helper class to select a pane before opening the {@link HubLayout}. */
public class HubShowPaneHelper {
    private @PaneId Integer mNextPaneId;

    public HubShowPaneHelper() {}

    // Sets the next pane id.
    public void setPaneToShow(@PaneId int nextPaneId) {
        assert mNextPaneId == null;
        mNextPaneId = nextPaneId;
    }

    /**
     * Gets the next pane id.
     *
     * <p>1) incognito mode: returns PaneId.INCOGNITO_TAB_SWITCHER;
     *
     * <p>2) regular mode: returns |mNextPaneId| or PaneId.TAB_SWITCHER if |mNextPaneId| isn't set.
     *
     * @param isIncognito Whether it is in the incognito mode.
     */
    @PaneId
    public int getNextPaneId(boolean isIncognito) {
        if (isIncognito) {
            return PaneId.INCOGNITO_TAB_SWITCHER;
        }

        if (mNextPaneId == null) {
            return PaneId.TAB_SWITCHER;
        }

        return mNextPaneId;
    }

    /**
     * Returns the next pane id and resets the |mNextPaneId| to be null.
     *
     * @param isIncognito Whether it is in the incognito mode.
     */
    @PaneId
    public int consumeNextPaneId(boolean isIncognito) {
        int nextPaneId = getNextPaneId(isIncognito);

        // Resets the mNextPaneId.
        mNextPaneId = null;
        return nextPaneId;
    }

    Integer getNextPaneIdForTesting() {
        return mNextPaneId;
    }
}
