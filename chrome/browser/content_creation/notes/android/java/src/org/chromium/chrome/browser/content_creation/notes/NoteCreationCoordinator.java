// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

/**
 * Public interface note creation component that is responsible for notes main UI and its
 * subcomponents.
 */
public interface NoteCreationCoordinator {
    /** Displays the main dialog for note creation. */
    void showDialog();
}
