// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;

/** The interface for communication between the {@link HubLayout} and Hub internals. */
public interface HubController {
    /** Called once by {@link HubLayout} when it is initialized. */
    void setHubLayoutController(@NonNull HubLayoutController hubLayoutController);

    /** Returns the view that contains all the Hub UI. */
    @NonNull
    HubContainerView getContainerView();

    /** Called at the start of {@link HubLayout#show(long, boolean)}. */
    void onHubLayoutShow();

    /** Called at the end of {@link HubLayout#doneHiding()}. */
    void onHubLayoutDoneHiding();

    /**
     * Called when the legacy back method {@link HubLayout#onBackPressed()} is invoked.
     *
     * @return whether the back press was handled.
     */
    boolean onHubLayoutBackPressed();
}
