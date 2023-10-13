// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

/** Interface for observing {@link HubLayoutAnimationRunner} animation phases. */
public interface HubLayoutAnimationListener {
    /** Called just before a {@link HubLayoutAnimationRunner} starts an animation. */
    default void beforeStart() {}

    /** Called when a {@link HubLayoutAnimationRunner} starts an animation. */
    default void onStart() {}

    /**
     * Called when a {@link HubLayoutAnimationRunner} ends an animation.
     *
     * @param wasForcedToFinish Whether the animation was forced to finish early.
     */
    default void onEnd(boolean wasForcedToFinish) {}
}
