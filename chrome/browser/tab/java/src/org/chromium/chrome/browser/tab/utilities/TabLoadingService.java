// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.utilities;

import androidx.annotation.IntDef;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
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
            new EmptyTabObserver() {
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

    private void onTabLoadFinished(Tab tab, @LoadResult int result) {
        ObserverList<LoadIfNeededCallback> callbacks = mQueuedTabs.remove(tab.getId());
        if (callbacks == null) return;

        tab.removeObserver(sObserver);
        for (LoadIfNeededCallback callback : callbacks) {
            callback.onLoadFinished(tab, result);
        }
    }

    public void clearForTesting() {
        mQueuedTabs.clear();
    }

    static void setInstanceForTesting(@Nullable TabLoadingService service) {
        sInstanceForTesting = service;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
