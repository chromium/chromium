// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;
import org.chromium.chrome.browser.xsurface.ProcessScope;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/** Tracker class for various feed surfaces. */
public class FeedSurfaceTracker implements SurfaceCoordinator.Observer {
    /** Feed surface tracker observer. */
    public interface Observer {
        // Called any time a Feed surface is opened.
        void surfaceOpened();
    }

    private static FeedSurfaceTracker sSurfaceTracker;

    // We avoid attaching surfaces until after |startup()| is called. This ensures that
    // the correct sign-in state is used if attaching the surface triggers a fetch.
    private boolean mStartupCalled;

    private ObserverList<Observer> mObservers = new ObserverList<>();

    // Tracks all the instances of FeedSurfaceCoordinator.
    @VisibleForTesting HashSet<SurfaceCoordinator> mCoordinators;

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
            for (SurfaceCoordinator coordinator : mCoordinators) {
                coordinator.onSurfaceOpened();
            }
        }
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    boolean isStartupCalled() {
        return mStartupCalled;
    }

    void untrackSurface(SurfaceCoordinator coordinator) {
        if (mCoordinators == null) {
            return;
        }
        mCoordinators.remove(coordinator);
        coordinator.removeObserver(this);
    }

    void trackSurface(SurfaceCoordinator coordinator) {
        if (mCoordinators == null) {
            mCoordinators = new HashSet<>();
        }
        mCoordinators.add(coordinator);
        coordinator.addObserver(this);
    }

    /** Clears all inactive coordinators and resets account. */
    public void clearAll() {
        if (mCoordinators == null) return;

        List<SurfaceCoordinator> activeCoordinators = new ArrayList<>();
        for (SurfaceCoordinator coordinator : mCoordinators) {
            if (coordinator.isActive()) activeCoordinators.add(coordinator);
        }

        for (SurfaceCoordinator coordinator : activeCoordinators) {
            coordinator.onSurfaceClosed();
        }

        ProcessScope processScope = getXSurfaceProcessScope();
        if (processScope != null) {
            processScope.resetAccount();
        }

        for (SurfaceCoordinator coordinator : activeCoordinators) {
            coordinator.onSurfaceOpened();
        }
    }

    @Override
    public void surfaceOpened() {
        for (Observer observer : mObservers) {
            observer.surfaceOpened();
        }
    }

    public void resetForTest() {
        mStartupCalled = false;
    }
}
