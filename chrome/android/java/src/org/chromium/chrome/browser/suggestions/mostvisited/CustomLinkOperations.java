// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

/** Methods to change and query custom links that exist alongside Most Visited links. */
@NullMarked
public interface CustomLinkOperations {
    /**
     * Adds a custom link with {@param name} and {@param url}, which must not collide with an
     * existing custom link.
     *
     * @param name The name of the added tile.
     * @param url The URL of the added tile.
     * @return Whether the operation successfully ran.
     */
    boolean addCustomLink(String name, @Nullable GURL url);

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
     * Queries the existence of a custom link identified by {@param keyUrl}.
     *
     * @param keyUrl The URL of an existing custom tile to query.
     * @return Whether a custom link identified by {@param keyUrl} exists.
     */
    boolean queryCustomLink(GURL keyUrl);
}
