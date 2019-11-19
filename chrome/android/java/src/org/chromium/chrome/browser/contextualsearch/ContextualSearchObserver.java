// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.gsa.GSAContextDisplaySelection;

/**
 * An interface to be notified when contextual searches are performed or no longer active.
 */
interface ContextualSearchObserver {
    /**
     * Notifies that a contextual search was performed, and provides the selection context if
     * the feature is fully enabled (and {@code null} otherwise).
     * This method may be called multiple times if the selection changes while Contextual Search is
     * showing.
     * NOTE: this context data can be quite privacy-sensitive because it contains text from the
     * page being viewed by the user, which may include sensitive or personal information.
     * Clients must follow standard privacy policy before logging or transmitting this information.
     * @param selectionContext The selection and context used for the Contextual Search, or
     *        {@code null} if the feature has not yet been fully enabled.
     */
    void onShowContextualSearch(@Nullable GSAContextDisplaySelection selectionContext);

    /**
     * Notifies that a contextual search is no longer in effect, and the results are no longer
     * available in the UX.
     */
    void onHideContextualSearch();
}
