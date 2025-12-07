// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

/** Methods to access Custom Links, which may exist alongside Most Visited links. */
@NullMarked
public interface CustomLinkOperations {
    /**
     * Adds a custom link with {@param name} and {@param url}, which must not collide with an
     * existing custom link.
     *
     * @param name The name of the added tile.
     * @param url The URL of the added tile.
     * @param pos The position to add tile, with null indicating add to end.
     * @return Whether the operation successfully ran.
     */
    boolean addCustomLink(String name, @Nullable GURL url, @Nullable Integer pos);

    /**
     * Assigns a link identified by {@param keyUrl} to a custom link specified with {@param name}
     * and {@param url}. This may modify an existing custom link, or convert a Most Visited link to
     * a custom link.
     *
     * @param keyUrl The URL of an existing Most Visited tile or custom tile to assign.
     * @param name The updated name of the tile.
     * @param url The updated URL of the tile.
     * @return Whether the operation successfully ran.
     */
    boolean assignCustomLink(GURL keyUrl, String name, @Nullable GURL url);

    /**
     * Deletes a custom link identified by {@param keyUrl}, no-op if non-existent.
     *
     * @param keyUrl The URL of an existing custom tile to delete.
     * @return Whether the operation successfully ran.
     */
    boolean deleteCustomLink(GURL keyUrl);

    /**
     * Tests the existence of a custom link identified by {@param keyUrl}.
     *
     * @param keyUrl The URL to search.
     * @return Whether a custom link identified by {@param keyUrl} exists.
     */
    boolean hasCustomLink(GURL keyUrl);

    /**
     * Moves a custom link identified by {@param keyUrl} to a new position, and shift all other
     * custom links between the old position and the new towards the former.
     *
     * @param keyUrl The URL of the custom link to move.
     * @param newPos The new position for the custom link to move to.
     * @return Whether the operation successfully ran.
     */
    boolean reorderCustomLink(GURL keyUrl, int newPos);
}
