// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.os.Handler;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.xsurface.ImageCacheHelper;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger.ClosedReason;

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

    private static final int CLEAR_FEED_CACHE_DEFAULT_DELAY_SECONDS_FOR_OPEN_CARD = 30;
    private static final int CLEAR_FEED_CACHE_DEFAULT_DELAY_SECONDS_FOR_LEAVE_FEED = 0;
    private static final int DO_NOT_CLEAR_FEED_CACHE = -1;
    private static final int ALWAYS_CLEAR_FEED_CACHE_REGARDLESS_OF_MEMORY = -1;

    private static FeedSurfaceTracker sSurfaceTracker;

    // We avoid attaching surfaces until after |startup()| is called. This ensures that
    // the correct sign-in state is used if attaching the surface triggers a fetch.
    private boolean mStartupCalled;

    private ObserverList<Observer> mObservers = new ObserverList<>();

    // Tracks all the instances of FeedSurfaceCoordinator.
    @VisibleForTesting HashSet<SurfaceCoordinator> mCoordinators;

    private final Handler mHandler;
    private final int mClearFeedCacheForOpenCardDelayMs;
    private final int mClearFeedCacheForLeaveFeedDelayMs;
    private Runnable mClearFeedCacheRunnable;

    public static FeedSurfaceTracker getInstance() {
        if (sSurfaceTracker == null) {
            sSurfaceTracker = new FeedSurfaceTracker();
        }
        return sSurfaceTracker;
    }

    private FeedSurfaceTracker() {
        boolean needToClearFeedCache = false;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_LOW_MEMORY_IMPROVEMENT)) {
            int physicalMemoryKB = SysUtils.amountOfPhysicalMemoryKB();
            int lowMemoryThreasholdMB =
                    ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                            ChromeFeatureList.FEED_LOW_MEMORY_IMPROVEMENT,
                            "low_memory_threshold",
                            ALWAYS_CLEAR_FEED_CACHE_REGARDLESS_OF_MEMORY);
            if ((lowMemoryThreasholdMB == ALWAYS_CLEAR_FEED_CACHE_REGARDLESS_OF_MEMORY)
                    || (physicalMemoryKB > 0 && physicalMemoryKB / 1024 <= lowMemoryThreasholdMB)) {
                needToClearFeedCache = true;
            }
        }
        if (needToClearFeedCache) {
            mHandler = new Handler();
            int value =
                    ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                            ChromeFeatureList.FEED_LOW_MEMORY_IMPROVEMENT,
                            "open_card_delay",
                            CLEAR_FEED_CACHE_DEFAULT_DELAY_SECONDS_FOR_OPEN_CARD);
            mClearFeedCacheForOpenCardDelayMs =
                    (value == DO_NOT_CLEAR_FEED_CACHE) ? DO_NOT_CLEAR_FEED_CACHE : value * 1000;
            value =
                    ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                            ChromeFeatureList.FEED_LOW_MEMORY_IMPROVEMENT,
                            "leave_feed_delay",
                            CLEAR_FEED_CACHE_DEFAULT_DELAY_SECONDS_FOR_LEAVE_FEED);
            mClearFeedCacheForLeaveFeedDelayMs =
                    (value == DO_NOT_CLEAR_FEED_CACHE) ? DO_NOT_CLEAR_FEED_CACHE : value * 1000;
        } else {
            mHandler = null;
            mClearFeedCacheForOpenCardDelayMs = DO_NOT_CLEAR_FEED_CACHE;
            mClearFeedCacheForLeaveFeedDelayMs = DO_NOT_CLEAR_FEED_CACHE;
        }
    }

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

        // Clear the feed cache when all feed surfaces are destroyed.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_LOW_MEMORY_IMPROVEMENT)
                && mCoordinators.isEmpty()) {
            @ClosedReason int closedReason = coordinator.getClosedReason();
            int delayMs = getClearFeedCacheDelayTimeMs(closedReason);
            if (delayMs == 0) {
                clearFeedCache();
            } else if (delayMs != DO_NOT_CLEAR_FEED_CACHE) {
                mClearFeedCacheRunnable = () -> clearFeedCache();
                mHandler.postDelayed(mClearFeedCacheRunnable, delayMs);
            }
        }
    }

    void trackSurface(SurfaceCoordinator coordinator) {
        if (mCoordinators == null) {
            mCoordinators = new HashSet<>();
        }
        mCoordinators.add(coordinator);
        coordinator.addObserver(this);

        // No need to clear the feed cache if a new feed surface is being created, i.e. the user
        // goes back to the NTP or opens a new NTP.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_LOW_MEMORY_IMPROVEMENT)) {
            if (mClearFeedCacheRunnable != null) {
                mHandler.removeCallbacks(mClearFeedCacheRunnable);
                mClearFeedCacheRunnable = null;
            }
        }
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

    private int getClearFeedCacheDelayTimeMs(@ClosedReason int closedReason) {
        if (closedReason == ClosedReason.OPEN_CARD) {
            return mClearFeedCacheForOpenCardDelayMs;
        } else if (closedReason == ClosedReason.LEAVE_FEED) {
            return mClearFeedCacheForLeaveFeedDelayMs;
        } else {
            // The user still stays in the feed for all other closed reasons. Returns -1 to mean
            // no clearing to feed cache.
            return DO_NOT_CLEAR_FEED_CACHE;
        }
    }

    private void clearFeedCache() {
        // Clear the feed image memory cache.
        ProcessScope processScope = getXSurfaceProcessScope();
        if (processScope != null) {
            ImageCacheHelper imageCacheHelper = processScope.provideImageCacheHelper();
            if (imageCacheHelper != null) {
                imageCacheHelper.clearMemoryCache();
            }
        }
    }
}
