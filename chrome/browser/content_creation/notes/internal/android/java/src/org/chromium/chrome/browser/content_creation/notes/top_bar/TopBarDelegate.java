// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes.top_bar;

/**
 * Interface used by the top bar to delegate actions back to another instance.
 */
public interface TopBarDelegate {
    /**
     * Invoked when the user signals a request for dismissal via the top bar.
     */
    public void dismiss();

    /**
     * Invoked when the user signals a request to publish the note.
     */
    public void publish();

    /**
     * Invoked when the user requests the main action to be executed
     * via the top bar.
     */
    public void executeAction();
}