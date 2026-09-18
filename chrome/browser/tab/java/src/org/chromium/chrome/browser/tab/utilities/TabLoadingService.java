// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.utilities;

import android.os.SystemClock;
import android.util.SparseArray;
import android.util.SparseIntArray;
import android.util.SparseLongArray;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;

/**
 * Service for queuing and monitoring tabs that need to be loaded on demand. Supports tracking
 * loading status and notifying registered callbacks when loading finishes.
 */
@NullMarked
public class TabLoadingService {

    /**
     * Possible outcomes of a tab load request. Used to indicate the final state of the tab when
     * notifying registered callbacks.
     */
    @IntDef({
        LoadResult.SUCCESS,
        LoadResult.FAILURE,
        LoadResult.CRASH,
        LoadResult.DESTROYED,
        LoadResult.CANCELLED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LoadResult {
        int SUCCESS = 0;
        int FAILURE = 1;
        int CRASH = 2;
        int DESTROYED = 3;
        int CANCELLED = 4;
    }

    /** Callback interface to be notified when a queued tab finishes loading or fails. */
    @FunctionalInterface
    public interface LoadIfNeededCallback {
        /**
         * Called when the tab load operation completes or fails.
         *
         * @param tab The tab that was being loaded.
         * @param result The final {@link LoadResult} of the load operation.
         */
        void onLoadFinished(Tab tab, @LoadResult int result);
    }

    private static final TabObserver sObserver =
            new TabObserver() {
                /**
                 * Handles early load completion on the first visually non-empty paint.
                 *
                 * <p>When {@link OnDemandBackgroundTabCaptureConfig#isEarlyFirstPaintEnabled()} is
                 * true, this triggers load completion as soon as the initial frame is rendered
                 * rather than waiting for full network and resource load completion.
                 *
                 * <p>If a non-zero delay buffer is configured via Finch ({@code
                 * first_paint_delay_ms}), completion is scheduled asynchronously. A monotonic
                 * generation token is captured to ensure stale tasks from previous attempts are
                 * discarded if the tab fails, crashes, or is reloaded during the delay window.
                 */
                @Override
                public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                    if (!OnDemandBackgroundTabCaptureConfig.isEarlyFirstPaintEnabled()) {
                        return;
                    }
                    TabLoadingService service = getInstance();
                    if (!OnDemandBackgroundTabCaptureConfig.hasFirstPaintDelay()) {
                        service.onTabLoadFinished(tab, LoadResult.SUCCESS);
                    } else {
                        int tabId = tab.getId();
                        int generation = service.getLoadGeneration(tabId);
                        if (generation == -1) {
                            return;
                        }
                        int delayMs = OnDemandBackgroundTabCaptureConfig.getFirstPaintDelayMs();
                        PostTask.postDelayedTask(
                                TaskTraits.UI_DEFAULT,
                                () -> {
                                    if (!tab.isDestroyed()
                                            && service.isLoadGenerationActive(tabId, generation)) {
                                        service.onTabLoadFinished(tab, LoadResult.SUCCESS);
                                    }
                                },
                                delayMs);
                    }
                }

                @Override
                public void onPageLoadFinished(Tab tab, GURL url) {
                    getInstance().onTabLoadFinished(tab, LoadResult.SUCCESS);
                }

                @Override
                public void onPageLoadFailed(Tab tab, int errorCode) {
                    getInstance().onTabLoadFinished(tab, LoadResult.FAILURE);
                }

                @Override
                public void onCrash(Tab tab) {
                    getInstance().onTabLoadFinished(tab, LoadResult.CRASH);
                }

                @Override
                public void onDestroyed(Tab tab) {
                    getInstance().onTabLoadFinished(tab, LoadResult.DESTROYED);
                }
            };

    private static class LazyHolder {
        private static final TabLoadingService INSTANCE = new TabLoadingService();
    }

    private static @Nullable TabLoadingService sInstanceForTesting;

    private final SparseArray<ObserverList<LoadIfNeededCallback>> mQueuedTabs = new SparseArray<>();

    /** Tabs that are actively occupying a concurrency slot and undergoing loading. */
    private final List<Tab> mLoadingTabs = new ArrayList<>();

    /**
     * Tabs that require reloading but are waiting in FIFO order until an active concurrency slot
     * opens up in {@link #mLoadingTabs}.
     */
    private final Deque<Tab> mPendingTabs = new ArrayDeque<>();

    /** Whether the concurrent load limit is enabled via feature flag. */
    private final boolean mLimitEnabled;

    /** The maximum number of tabs permitted to load concurrently. */
    private final int mLimit;

    /**
     * Maps active tab IDs to strictly increasing generation tokens. Used to invalidate stale
     * delayed runnables scheduled during post-first-paint delay windows.
     */
    private final SparseIntArray mTabLoadGenerations = new SparseIntArray();

    /**
     * Maps pending tab IDs to the timestamps (in {@link SystemClock#elapsedRealtime()}) when they
     * were enqueued in {@link #mPendingTabs}. Used to record queue wait duration once an active
     * concurrency slot becomes available.
     */
    private final SparseLongArray mTabQueueStartTimes = new SparseLongArray();

    /** Monotonic generation counter incremented with each new load request. */
    private int mNextGeneration;

    @VisibleForTesting
    TabLoadingService() {
        mLimitEnabled = OnDemandBackgroundTabCaptureConfig.isLimitConcurrentLoadsEnabled();
        mLimit = OnDemandBackgroundTabCaptureConfig.getConcurrentLoadLimit();
    }

    /** Returns the singleton instance of {@link TabLoadingService}. */
    public static TabLoadingService getInstance() {
        ThreadUtils.assertOnUiThread();
        return sInstanceForTesting != null ? sInstanceForTesting : LazyHolder.INSTANCE;
    }

    /**
     * Queues a tab for loading if needed.
     *
     * @param tab The tab to potentially load.
     * @return true if the tab was queued for reloading, returns false if the tab is already loaded.
     */
    public boolean queueLoadIfNeeded(Tab tab) {
        ThreadUtils.assertOnUiThread();
        if (mQueuedTabs.get(tab.getId()) != null) {
            return true;
        }

        if (!mLimitEnabled) {
            mQueuedTabs.put(tab.getId(), new ObserverList<>());
            return startTabLoad(tab, /* notifyOnFailure= */ false);
        }

        // Do not queue tabs that are already fully loaded and idle. Returning false early
        // avoids holding a slot in mPendingTabs or blocking other queued tabs when no load
        // is needed.
        if (!tab.isLoading()
                && !tab.isFrozen()
                && tab.getPendingLoadParams() == null
                && !tab.needsReload()) {
            return false;
        }

        // Track in mQueuedTabs before loading or queueing so subsequent caller invocations
        // of addLoadIfNeededCallback() can register listeners (including for tabs that were
        // already loading).
        mQueuedTabs.put(tab.getId(), new ObserverList<>());

        if (mPendingTabs.isEmpty() && mLoadingTabs.size() < mLimit) {
            RecordHistogram.recordCount100Histogram(
                    "Android.TabLoadingService.PendingQueueDepth", 0);
            return startTabLoad(tab, /* notifyOnFailure= */ false);
        } else {
            tab.addObserver(sObserver);
            mPendingTabs.add(tab);
            mTabQueueStartTimes.put(tab.getId(), SystemClock.elapsedRealtime());
            RecordHistogram.recordCount100Histogram(
                    "Android.TabLoadingService.PendingQueueDepth", mPendingTabs.size());
            return true;
        }
    }

    /**
     * Registers a listener for the tab to be loaded.
     *
     * @param tab The tab to observe.
     * @param callback The callback to be notified when loading finishes.
     * @return true if added successfully. Returns false if the tab is not queued for loading.
     */
    public boolean addLoadIfNeededCallback(Tab tab, LoadIfNeededCallback callback) {
        ThreadUtils.assertOnUiThread();
        ObserverList<LoadIfNeededCallback> callbacks = mQueuedTabs.get(tab.getId());
        if (callbacks == null) {
            return false;
        }
        callbacks.addObserver(callback);
        return true;
    }

    /**
     * Removes the LoadIfNeededCallback from the tab.
     *
     * @param tab The tab to remove the callback from.
     * @param callback The callback to remove.
     * @return true if a callback was found and removed.
     */
    public boolean removeLoadIfNeededCallback(Tab tab, LoadIfNeededCallback callback) {
        ThreadUtils.assertOnUiThread();
        ObserverList<LoadIfNeededCallback> callbacks = mQueuedTabs.get(tab.getId());
        if (callbacks == null) {
            return false;
        }
        return callbacks.removeObserver(callback);
    }

    /**
     * Checks if a tab is currently queued for loading.
     *
     * @param tabId The ID of the tab to check.
     * @return true if a previous request to load the tab is already queued.
     */
    public boolean isTabQueuedForLoad(int tabId) {
        ThreadUtils.assertOnUiThread();
        return mQueuedTabs.get(tabId) != null;
    }

    /**
     * Cancels an in-flight or queued load for the given tab.
     *
     * <p>If the tab is actively loading, this aborts the load and sets {@code needsReload} to true
     * to preserve dirty state for future navigations. Any waiting pending tabs are immediately
     * scheduled.
     *
     * <p>Registered callbacks are notified with {@link LoadResult#CANCELLED} so consumers can tear
     * down any UI they put up for the load.
     *
     * <p><b>Cancellation is global and immediate.</b> Loads are keyed by tab and are not reference
     * counted, so a caller that cancels a tab also cancels it for every other component waiting on
     * that same tab. Callers that cancel speculatively, or on behalf of a UI whose state the user
     * may still discard, must therefore be sure no other component still needs the load.
     *
     * @param tab The tab to cancel loading for.
     * @return true if an active or pending load for the tab was cancelled.
     */
    public boolean cancelLoadIfNeeded(Tab tab) {
        ThreadUtils.assertOnUiThread();
        boolean removedPending = mPendingTabs.remove(tab);
        mTabLoadGenerations.delete(tab.getId());
        mTabQueueStartTimes.delete(tab.getId());
        boolean wasQueued = mQueuedTabs.get(tab.getId()) != null;
        boolean wasLoading = mLoadingTabs.remove(tab);

        // The observer is attached to both actively loading and pending tabs, so it must be
        // detached in either case to avoid observing tabs the service no longer tracks.
        if (wasLoading || removedPending) {
            tab.removeObserver(sObserver);
        }

        // If the tab is currently activated (in foreground), do not stop its load or mark it for
        // reload as the user is actively viewing it.
        if (wasLoading && !tab.isDestroyed() && !tab.isActivated()) {
            tab.stopLoading();
            WebContents webContents = tab.getWebContents();
            if (webContents != null && !webContents.isDestroyed()) {
                // The load was aborted midway, so the tab holds partially rendered content.
                // Marking it for reload ensures a later activation re-fetches the page rather
                // than surfacing that half-loaded state to the user.
                NavigationController navigationController = webContents.getNavigationController();
                if (navigationController != null) {
                    navigationController.setNeedsReload();
                }
            }
        }

        // Notify before scheduling pending tabs so consumers observe the cancellation while the
        // service state is already consistent. This also clears the tab from mQueuedTabs.
        removeCallbacksAndNotify(tab, LoadResult.CANCELLED);

        if (wasLoading) {
            maybeLoadQueuedTabs();
            return true;
        }
        return removedPending || wasQueued;
    }

    private boolean startTabLoad(Tab tab, boolean notifyOnFailure) {
        // If tab was destroyed while waiting in the queue, notify destroyed immediately.
        if (tab.isDestroyed()) {
            if (notifyOnFailure) {
                removeCallbacksAndNotify(tab, LoadResult.DESTROYED);
            } else {
                mQueuedTabs.delete(tab.getId());
            }
            return false;
        }

        mTabLoadGenerations.put(tab.getId(), ++mNextGeneration);
        boolean loadIfNeededResult = tab.loadIfNeeded(/* forceBackingSize= */ true);
        boolean isLoadingResult = tab.isLoading();
        if (!loadIfNeededResult || !isLoadingResult) {
            mTabLoadGenerations.delete(tab.getId());
            if (notifyOnFailure) {
                int result = tab.isDestroyed() ? LoadResult.DESTROYED : LoadResult.FAILURE;
                removeCallbacksAndNotify(tab, result);
            } else {
                mQueuedTabs.delete(tab.getId());
            }
            return false;
        }
        tab.addObserver(sObserver);
        mLoadingTabs.add(tab);
        return true;
    }

    /** Returns the active load generation token for the tab, or -1 if not queued. */
    private int getLoadGeneration(int tabId) {
        return mTabLoadGenerations.get(tabId, -1);
    }

    /** Returns whether the given load generation token is currently active for the tab. */
    private boolean isLoadGenerationActive(int tabId, int generation) {
        int activeGen = getLoadGeneration(tabId);
        return activeGen != -1 && activeGen == generation;
    }

    private void onTabLoadFinished(Tab tab, @LoadResult int result) {
        mLoadingTabs.remove(tab);
        mPendingTabs.remove(tab);
        mTabLoadGenerations.delete(tab.getId());
        mTabQueueStartTimes.delete(tab.getId());
        tab.removeObserver(sObserver);

        removeCallbacksAndNotify(tab, result);
        maybeLoadQueuedTabs();
    }

    /** Removes the callbacks for the tab and notifies them of the load result. */
    private void removeCallbacksAndNotify(Tab tab, @LoadResult int result) {
        ObserverList<LoadIfNeededCallback> callbacks = mQueuedTabs.get(tab.getId());
        mQueuedTabs.delete(tab.getId());
        if (callbacks != null) {
            for (LoadIfNeededCallback callback : callbacks) {
                callback.onLoadFinished(tab, result);
            }
        }
    }

    private void maybeLoadQueuedTabs() {
        if (!mLimitEnabled) {
            return;
        }

        while (mLoadingTabs.size() < mLimit && !mPendingTabs.isEmpty()) {
            Tab nextTab = mPendingTabs.removeFirst();
            long queueStartTime = mTabQueueStartTimes.get(nextTab.getId(), 0);
            if (queueStartTime > 0) {
                mTabQueueStartTimes.delete(nextTab.getId());
                long duration = SystemClock.elapsedRealtime() - queueStartTime;
                RecordHistogram.recordMediumTimesHistogram(
                        "Android.TabLoadingService.QueueWaitDuration", duration);
            }
            startTabLoad(nextTab, /* notifyOnFailure= */ true);
        }
    }

    /** Clears all queued and loading state for testing. */
    public void clearForTesting() {
        mQueuedTabs.clear();
        mTabLoadGenerations.clear();
        mTabQueueStartTimes.clear();
        mNextGeneration = 0;
        mLoadingTabs.clear();
        mPendingTabs.clear();
    }

    /** Sets the singleton instance of {@link TabLoadingService} for testing. */
    public static void setInstanceForTesting(@Nullable TabLoadingService service) {
        sInstanceForTesting = service;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
