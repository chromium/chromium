// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.sessionrestore;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.ObserverList;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * Main coordinator for SessionRestoreManager. This class take charge in putting tab into, and take
 * out from {@link TabFreezer}, which dedicate to putting tab into freezing / unfrozen mode.
 *
 * This class handles:
 * - Handle eviction timeout
 * - Manage TabInteractionRecorder states when store / restore
 * - Open for observers and dispatch events
 */
public class SessionRestoreManagerImpl implements SessionRestoreManager {
    private static final long DEFAULT_EVICTION_TIMEOUT = TimeUnit.MINUTES.toMillis(5);

    // TODO(wenyufu): Add histogram for cache being cleaned and make these @IntDef
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({EvictionReason.CLIENT_CLEAN, EvictionReason.TIMEOUT, EvictionReason.NEW_TAB_OVERRIDE})
    @interface EvictionReason {
        int CLIENT_CLEAN = 0;
        int TIMEOUT = 1;
        int NEW_TAB_OVERRIDE = 2;
    }

    private final TabFreezer mTabFreezer;
    private final ObserverList<SessionRestoreManager.Observer> mObservers = new ObserverList<>();

    private @Nullable CallbackController mCallbackController;

    private long mEvictionTimeout = DEFAULT_EVICTION_TIMEOUT;

    public SessionRestoreManagerImpl() {
        this(new TabFreezer());
    }

    @VisibleForTesting
    SessionRestoreManagerImpl(TabFreezer testTebFreezer) {
        mTabFreezer = testTebFreezer;
    }

    @Override
    public boolean store(Tab tabToStore) {
        if (mTabFreezer.hasTab()) {
            cancelEvictionTimer();
            clearCacheWithReason(EvictionReason.NEW_TAB_OVERRIDE);
        }

        TabInteractionRecorder recorder = TabInteractionRecorder.getFromTab(tabToStore);
        assert recorder != null : "Tab to store have to has an interaction recorder.";

        // Manually record the interaction related prefs for the storing tab.
        ReparentingTask.from(tabToStore).detach();
        recorder.onTabClosing();

        boolean success = mTabFreezer.freeze(tabToStore);
        if (!success) {
            return false;
        }

        if (mEvictionTimeout > 0) {
            startEvictionTimer();
        }
        return true;
    }

    @Nullable
    @Override
    public Tab restoreTab() {
        if (!canRestoreTab()) return null;
        cancelEvictionTimer();
        Tab tab = mTabFreezer.unfreeze();

        TabInteractionRecorder recorder = TabInteractionRecorder.getFromTab(tab);
        assert recorder != null : "Tab to restore has to have an interaction recorder.";
        recorder.reset();

        return tab;
    }

    @Override
    public boolean canRestoreTab() {
        return mTabFreezer.hasTab();
    }

    @Override
    public void setEvictionTimeout(long timeoutMs) {
        mEvictionTimeout = timeoutMs;
    }

    @Override
    public void cancelEvictionTimer() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
    }

    @Override
    public void clearCache() {
        if (!mTabFreezer.hasTab()) return;
        cancelEvictionTimer();
        clearCacheWithReason(EvictionReason.CLIENT_CLEAN);
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    private void startEvictionTimer() {
        assert mCallbackController == null : "An eviction timer is in place.";
        mCallbackController = new CallbackController();
        PostTask.postDelayedTask(TaskTraits.BEST_EFFORT,
                mCallbackController.makeCancelable(
                        () -> clearCacheWithReason(EvictionReason.TIMEOUT)),
                mEvictionTimeout /*ms*/);
    }

    private void clearCacheWithReason(int reason) {
        int tabId = mTabFreezer.clear();
        // TODO(https://crbug.com/1385960): Remove this line if clean up is not required.
        AsyncTabParams param =
                AsyncTabParamsManagerSingleton.getInstance().getAsyncTabParams().get(tabId);
        if (param instanceof TabReparentingParams) {
            AsyncTabParamsManagerSingleton.getInstance().remove(tabId);
        }

        for (Observer obs : mObservers) {
            obs.onCacheCleared(reason);
        }
    }
}
