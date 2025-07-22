// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.view.MotionEvent;
import android.view.View;

import org.chromium.build.annotations.NullMarked;

/** Delegate for Most Visited tile drag UI. */
@NullMarked
interface TileDragDelegate {
    /**
     * Handler for ACTION_DOWN touch event on tile. This may start a tile drag session.
     *
     * @param view The View of the tile receiving ACTION_DOWN.
     * @param event The ACTION_DOWN event.
     * @param eventListener Listener for tile drag UI events.
     */
    void onTileTouchDown(View view, MotionEvent event, TileDragSession.EventListener eventListener);

    /**
     * Handler for non-ACTION_DOWN events to continue / end a tile drag session. Should be called if
     * a tile drag session is live.
     */
    void onSessionTileTouch(View view, MotionEvent event);

    /** Returns whether a tile drag session is live, and onSessionTileTouch() needs to be called. */
    boolean hasSession();

    /**
     * Shows the divider that separates Custom Tiles and Top Sites Tiles.
     *
     * @param isAnimated Whether to animate the transition.
     */
    void showDivider(boolean isAnimated);

    /**
     * Hides the divider that separates Custom Tiles and Top Sites Tiles.
     *
     * @param isAnimated Whether to animate the transition.
     */
    void hideDivider(boolean isAnimated);

    /** Forces tile drag session to end. */
    void reset();
}
