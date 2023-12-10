// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.Nullable;

import org.chromium.url.GURL;

/**
 * An interface for network communication between the Contextual Search client and server.
 * This is used to stub out the server during tests but in normal operation it just
 * short circuits from the {@link ContextualSearchManager} to itself.
 */
interface ContextualSearchNetworkCommunicator {
    /**
     * Starts a Search Term Resolution request.
     * When the response comes back {@link #handleSearchTermResolutionResponse} will be called.
     * @param selection the current selected text.
     * @param isExactResolve Whether the resolution should be restricted to an exact match with
     *        the given selection that cannot be expanded based on the response.
     * @param searchContext The {@link ContextualSearchContext} that the search will use.
     */
    void startSearchTermResolutionRequest(
            String selection, boolean isExactResolve, ContextualSearchContext searchContext);

    /**
     * Handles a Search Term Resolution response.
     * @param resolvedSearchTerm A {@link ResolvedSearchTerm} that encapsulates the response from
     *        the server.
     */
    void handleSearchTermResolutionResponse(ResolvedSearchTerm resolvedSearchTerm);

    /** Stops any navigation in the overlay panel's {@code WebContents}. */
    void stopPanelContentsNavigation();

    // --------------------------------------------------------------------------------------------
    // These are non-network actions that need to be stubbed out for testing.
    // --------------------------------------------------------------------------------------------

    /**
     * Gets the URL of the base page.
     * TODO(donnd): move to another interface, or rename this interface:
     * This is needed to stub out for testing, but has nothing to do with networking.
     * @return The URL of the base page (needed for testing purposes).
     */
    @Nullable
    GURL getBasePageUrl();
}
