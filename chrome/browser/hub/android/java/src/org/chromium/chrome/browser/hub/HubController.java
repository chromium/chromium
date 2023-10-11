// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;

/** The interface for communication between the {@link HubLayout} and Hub internals. */
public interface HubController {
    /** Returns the view that contains all the Hub UI. */
    public @NonNull HubContainerView getContainerView();

    /** Called at the start of {@link HubLayout#show(long, boolean)}. */
    public void onHubLayoutShow();

    /** Called at the end of {@link HubLayout#doneHiding()}. */
    public void onHubLayoutDoneHiding();
}
