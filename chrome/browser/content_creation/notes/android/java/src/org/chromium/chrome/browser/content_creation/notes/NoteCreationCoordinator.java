// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.app.Activity;

/**
 * Public interface note creation component that is responsible for notes main UI and its
 * subcomponents.
 */
public interface NoteCreationCoordinator {
    /**
     * Initializes the note creation coordinator and its sub components.
     * @param activity A {@link Activity} to create views and retrieve resources.
     */
    void initialize(Activity activity);

    /**
     * Displays the main dialog for note creation.
     */
    void showDialog();
}