// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;
import org.chromium.chrome.browser.profiles.Profile;

/** The delegate of the {@link FeedSurfaceProvider} creator needs to implement. */
@NullMarked
public interface FeedSurfaceDelegate {
    /**
     * Creates {@link FeedSurfaceLifecycleManager} for the specified {@link FeedSurfaceCoordinator}
     * in the {@link Activity}.
     *
     * @param activity The associated {@link Activity} of the {@link FeedSurfaceCoordinator}.
     * @param coordinator The coordinator managing the feed surface.
     * @return The {@link FeedSurfaceLifecycleManager}.
     */
    FeedSurfaceLifecycleManager createStreamLifecycleManager(
            Activity activity, SurfaceCoordinator coordinator, Profile profile);

    /** Notifies the delegate of a motion event. */
    void sendMotionEventForInputTracking(MotionEvent ev);

    /**
     * Checks whether the delegate want to intercept the given touch event.
     *
     * @param ev The given {@link MotionEvent}
     * @return True if the delegate want to intercept the event, otherwise return false.
     */
    boolean onInterceptTouchEvent(MotionEvent ev);
}
