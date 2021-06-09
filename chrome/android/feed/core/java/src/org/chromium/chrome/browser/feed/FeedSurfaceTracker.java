// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.v2.FeedServiceBridgeDelegateImpl;
import org.chromium.chrome.browser.xsurface.ProcessScope;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/**
 * Tracker class for various feed surfaces.
 */
public class FeedSurfaceTracker {
    private static FeedSurfaceTracker sSurfaceTracker;

    // We avoid attaching surfaces until after |startup()| is called. This ensures that
    // the correct sign-in state is used if attaching the surface triggers a fetch.
    private boolean mStartupCalled;

    private boolean mSetServiceBridgeDelegate;

    /**
     * Initializes the FeedServiceBridge. We do this once at startup, either in startup(), or
     * in FeedStreamSurface's constructor, whichever comes first.
     */
    void initServiceBridge() {
        if (mSetServiceBridgeDelegate) return;
        mSetServiceBridgeDelegate = true;
        FeedServiceBridge.setDelegate(new FeedServiceBridgeDelegateImpl());
    }

    // Tracks all the instances of FeedSurfaceCoordinator.
    @VisibleForTesting
    HashSet<FeedSurfaceCoordinator> mCoordinators;

    public static FeedSurfaceTracker getInstance() {
        if (sSurfaceTracker == null) {
            sSurfaceTracker = new FeedSurfaceTracker();
        }
        return sSurfaceTracker;
    }

    private FeedSurfaceTracker() {}

    /**
     * @return the process scope for the feed.
     */
    public ProcessScope getXSurfaceProcessScope() {
        return FeedServiceBridge.xSurfaceProcessScope();
    }

    /** Warms up the feed native components. */
    public void startup() {
        if (mStartupCalled) return;
        mStartupCalled = true;
        FeedServiceBridge.startup();
        if (mCoordinators != null) {
            for (FeedSurfaceCoordinator coordinator : mCoordinators) {
                coordinator.onSurfaceOpened();
            }
        }
    }

    boolean isStartupCalled() {
        return mStartupCalled;
    }

    void untrackSurface(FeedSurfaceCoordinator coordinator) {
        if (mCoordinators != null) {
            mCoordinators.remove(coordinator);
        }
    }

    void trackSurface(FeedSurfaceCoordinator coordinator) {
        if (mCoordinators == null) {
            mCoordinators = new HashSet<>();
        }
        mCoordinators.add(coordinator);
    }

    /** Clears all inactive coordinators and resets account. */
    public void clearAll() {
        if (mCoordinators == null) return;

        List<FeedSurfaceCoordinator> activeCoordinators = new ArrayList<>();
        for (FeedSurfaceCoordinator coordinator : mCoordinators) {
            if (coordinator.isActive()) activeCoordinators.add(coordinator);
        }

        for (FeedSurfaceCoordinator coordinator : activeCoordinators) {
            coordinator.onSurfaceClosed();
        }

        ProcessScope processScope = getXSurfaceProcessScope();
        if (processScope != null) {
            processScope.resetAccount();
        }

        for (FeedSurfaceCoordinator coordinator : activeCoordinators) {
            coordinator.onSurfaceOpened();
        }
    }

    @VisibleForTesting
    public void resetForTest() {
        mStartupCalled = false;
        mSetServiceBridgeDelegate = false;
    }
}
