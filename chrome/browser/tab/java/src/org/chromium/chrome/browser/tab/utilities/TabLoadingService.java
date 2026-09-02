// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.utilities;

import android.util.SparseIntArray;

import androidx.annotation.IntDef;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

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
    @IntDef({LoadResult.SUCCESS, LoadResult.FAILURE, LoadResult.CRASH, LoadResult.DESTROYED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LoadResult {
        int SUCCESS = 0;
        int FAILURE = 1;
        int CRASH = 2;
        int DESTROYED = 3;
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

    private final Map<Integer, ObserverList<LoadIfNeededCallback>> mQueuedTabs = new HashMap<>();

    /**
     * Maps active tab IDs to strictly increasing generation tokens. Used to invalidate stale
     * delayed runnables scheduled during post-first-paint delay windows.
     */
    private final SparseIntArray mTabLoadGenerations = new SparseIntArray();

    /** Monotonic generation counter incremented with each new load request. */
    private int mNextGeneration;

    private TabLoadingService() {}

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
        if (mQueuedTabs.containsKey(tab.getId())) {
            return true;
        }
        if (!tab.loadIfNeeded(/* forceBackingSize= */ true)) {
            return false;
        }
        if (!tab.isLoading()) {
            return false;
        }

        tab.addObserver(sObserver);
        mQueuedTabs.put(tab.getId(), new ObserverList<>());
        mTabLoadGenerations.put(tab.getId(), ++mNextGeneration);
        return true;
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
        return mQueuedTabs.containsKey(tabId);
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
        mTabLoadGenerations.delete(tab.getId());
        ObserverList<LoadIfNeededCallback> callbacks = mQueuedTabs.remove(tab.getId());
        if (callbacks == null) {
            return;
        }

        tab.removeObserver(sObserver);
        for (LoadIfNeededCallback callback : callbacks) {
            callback.onLoadFinished(tab, result);
        }
    }

    public void clearForTesting() {
        mQueuedTabs.clear();
        mTabLoadGenerations.clear();
        mNextGeneration = 0;
    }

    static void setInstanceForTesting(@Nullable TabLoadingService service) {
        sInstanceForTesting = service;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
