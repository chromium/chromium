// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions.toolbar;

/**
 * Interface used by the toolbar to delegate action handlers for the toolbar controls.
 */
public interface ToolbarControlsDelegate {
    /**
     * Invoked when the user taps the Cancel button.
     */
    public void cancelButtonTapped();

    /**
     * Invoked when the user taps the Done button.
     */
    public void doneButtonTapped();
}