// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** The interface for communication between the {@link HubLayout} and Hub internals. */
@NullMarked
public interface HubController {
    /** Called once by {@link HubLayout} when it is initialized. */
    void setHubLayoutController(HubLayoutController hubLayoutController);

    /** Returns the view that contains all the Hub UI. */
    HubContainerView getContainerView();

    /**
     * Returns the view that contains all the Hub UI. This is not guaranteed to be in a valid state
     * and should only be used when the Hub is being destroyed.
     */
    HubContainerView getContainerViewUnchecked();

    /** Returns the view that contains the Hub panes. */
    @Nullable View getPaneHostView();

    /** Returns the background color of the Hub for the pane. */
    @ColorInt
    int getBackgroundColor(@Nullable Pane pane);

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

    /** Returns the color mixer for the Hub. */
    HubColorMixer getHubColorMixer();
}
