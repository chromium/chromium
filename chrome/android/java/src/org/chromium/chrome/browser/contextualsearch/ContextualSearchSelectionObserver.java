// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

/**
 * An interface to be notified when contextual search selection is performed. Should be used
 * internally in Chrome with caution.
 */
public interface ContextualSearchSelectionObserver {

    /**
     * Notifies that a contextual search was performed, and provides the selection context. NOTE:
     * This method will be called no matter if the contextual search feature is fully enabled or not
     * and should be used with caution and ONLY BY CLIENTS PROCESSING THE SELECTION INFORMATION
     * LOCALLY. The context data can be quite privacy-sensitive because it contains text from the
     * page being viewed by the user, which may include sensitive or personal information. Clients
     * must follow standard privacy policy before logging or transmitting this information.
     *
     * <p>Clients subscribed to this method should ignore onShowContextualSearch() method.
     *
     * @param selection The selection and context used for the Contextual Search, or {@code null} if
     *     the feature has not yet been fully enabled.
     */
    void onSelectionChanged(ContextualSearchSelection selection);
}
