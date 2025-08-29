// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * UI logic to reorder Most Visited Tiles, moving a "from" tile to a "to" tile's location. Two
 * separate flows are supported:
 *
 * <ol>
 *   <li>Drag Flow: Runs a stateful "tile drag session" by handling touch events and managing the a
 *       state machine of per-session states.
 *   <li>Swap Flow: Fire and forget.
 * </ol>
 */
@NullMarked
interface TileDragDelegate {

    /** Different ways in which Tile reorder UI is accomplished. */
    @IntDef({
        ReorderFlow.DRAG_FLOW,
        ReorderFlow.SWAP_FLOW,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ReorderFlow {
        int DRAG_FLOW = 0;
        int SWAP_FLOW = 1;
        int NUM_ENTRIES = 2;
    }

    /**
     * Drag Flow conditional entrance: Handles ACTION_DOWN touch event on tile, and may start a tile
     * tile drag session.
     *
     * @param view The View of the tile receiving ACTION_DOWN.
     * @param event The ACTION_DOWN event.
     * @param eventListener Listener for tile drag UI events.
     */
    void onTileTouchDown(View view, MotionEvent event, TileDragSession.EventListener eventListener);

    /**
     * Drag Flow continuation: Handles for non-ACTION_DOWN events to continue / end a tile drag
     * session. Called while a tile drag session is live.
     */
    void onSessionTileTouch(View view, MotionEvent event);

    /** Returns whether a tile drag session is live, and onSessionTileTouch() needs to be called. */
    boolean hasTileDragSession();

    /**
     * Swap Flow run: Swaps a tile with another tile specified by its relative index.
     *
     * @param fromView The first tile to swap.
     * @param toDeltaIndex The relative index of the second tile to swap.
     * @param eventListener Listener for tile drag UI events.
     */
    void swapTiles(View fromView, int toDeltaIndex, TileDragSession.EventListener eventListener);

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

    /** Returns whether the {@param tileView} is the first among draggable tiles. */
    boolean isFirstDraggableTile(View tileView);

    /** Returns whether the {@param tileView} is the last among draggable tiles. */
    boolean isLastDraggableTile(View tileView);
}
