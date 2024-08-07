// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

/** An interface to be notified when contextual searches are performed or are no longer active. */
public interface ContextualSearchObserver {
    /**
     * Notifies that a contextual search was performed, and provides the selection context if the
     * feature is fully enabled (and {@code null} otherwise). This method may be called multiple
     * times if the selection changes while Contextual Search is showing.
     */
    void onShowContextualSearch();

    /**
     * Notifies that a contextual search is no longer in effect, and the results are no longer
     * available in the UX.
     */
    void onHideContextualSearch();
}
