// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Activity;
import android.view.WindowManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Set;

/** Helper class that keeps the screen on while an Actor task is active. */
@NullMarked
public class ActorTaskHelper implements ActorKeyedService.Observer, StartStopWithNativeObserver {
    private final Activity mActivity;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final Callback<Profile> mProfileObserver = (p) -> updateKeepScreenOn();
    private @Nullable ActorKeyedService mActorService;
    private boolean mKeepScreenOn;
    private @Nullable Tab mActingTab;

    /**
     * @param activity The {@link Activity} to manage flags for.
     * @param profileSupplier Supplier for the current {@link Profile}.
     * @param tabModelSelectorSupplier Supplier for the current {@link TabModelSelector}.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events.
     */
    public ActorTaskHelper(
            Activity activity,
            MonotonicObservableSupplier<Profile> profileSupplier,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mActivity = activity;
        mProfileSupplier = profileSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);
        mActivityLifecycleDispatcher.register(this);
    }

    @Override
    public void onTaskStateChanged(int taskId, @ActorTaskState int newState) {
        updateKeepScreenOn();
        if (mActingTab != null && ActorUtils.isCompletedState(newState)) {
            OffscreenRenderingManager.getInstance().stopOffscreenRendering(mActingTab);
            mActingTab = null;
        }
    }

    private void updateKeepScreenOn() {
        boolean shouldKeepScreenOn = shouldKeepScreenOn();
        if (shouldKeepScreenOn != mKeepScreenOn) {
            mKeepScreenOn = shouldKeepScreenOn;
            // TODO (b/502331292) : Handle setting/unsetting this flag with ReadAloud.
            if (mKeepScreenOn) {
                mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            } else {
                mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        }
    }

    private boolean shouldKeepScreenOn() {
        boolean[] hasActiveTask = new boolean[1];
        forEachRunningTask(task -> hasActiveTask[0] = true);
        return hasActiveTask[0];
    }

    @Override
    public void onStartWithNative() {
        if (mActingTab != null) {
            OffscreenRenderingManager.getInstance().stopOffscreenRendering(mActingTab);
            mActingTab = null;
        }
    }

    @Override
    public void onStopWithNative() {
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            // TODO(b/537362347): Update method to remove usage of getCurrentActingTab() when
            // refactoring for multi-task.
            mActingTab = getCurrentActingTab();
            if (mActingTab != null) {
                OffscreenRenderingManager.getInstance()
                        .startOffscreenRendering(
                                mActingTab,
                                mActivity.findViewById(android.R.id.content).getWidth(),
                                mActivity.findViewById(android.R.id.content).getHeight());
            }
        } else {
            forEachRunningTask(
                    task -> {
                        if (isTaskInCurrentWindow(task)) {
                            task.pause();
                        }
                    });
        }
    }

    @VisibleForTesting
    boolean isTaskInCurrentWindow(ActorTask task) {
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (selector == null) return false;
        for (int tabId : task.getTabs()) {
            if (selector.getTabById(tabId) != null) {
                return true;
            }
        }
        return false;
    }

    private void forEachActiveTask(Callback<ActorTask> callback) {
        ActorKeyedService service = maybeGetActorService();
        if (service == null) return;
        for (ActorTask task : service.getActiveTasks()) {
            callback.onResult(task);
        }
    }

    private void forEachRunningTask(Callback<ActorTask> callback) {
        forEachActiveTask(
                task -> {
                    if (ActorUtils.isRunningState(task.getState())) {
                        callback.onResult(task);
                    }
                });
    }

    private @Nullable ActorKeyedService maybeGetActorService() {
        Profile profile = mProfileSupplier.get();
        if (profile == null) return null;

        profile = profile.getOriginalProfile();
        ActorKeyedService currentService = ActorKeyedServiceFactory.getForProfile(profile);

        if (currentService != mActorService) {
            if (mActorService != null) mActorService.removeObserver(this);
            mActorService = currentService;
            if (mActorService != null) mActorService.addObserver(this);
        }
        return mActorService;
    }

    /** Stops any active Actor tasks that belong to this window when the activity is destroyed. */
    public void onDestroy() {
        forEachActiveTask(
                task -> {
                    if (isTaskInCurrentWindow(task) && mActorService != null) {
                        mActorService.stopTask(task.getId(), StoppedReason.SHUTDOWN);
                    }
                });
    }

    /** Cleans up the helper, removing observers and clearing flags. */
    public void destroy() {
        if (mActingTab != null) {
            OffscreenRenderingManager.getInstance().stopOffscreenRendering(mActingTab);
            mActingTab = null;
        }
        mActivityLifecycleDispatcher.unregister(this);
        mProfileSupplier.removeObserver(mProfileObserver);
        if (mActorService != null) {
            mActorService.removeObserver(this);
            mActorService = null;
        }
        if (mKeepScreenOn) {
            mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            mKeepScreenOn = false;
        }
    }

    private @Nullable Tab getCurrentActingTab() {
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (selector == null) return null;

        int tabId = getActiveTaskLastActedTabId();
        return (tabId != Tab.INVALID_TAB_ID) ? selector.getTabById(tabId) : null;
    }

    private int getActiveTaskLastActedTabId() {
        maybeGetActorService();
        if (mActorService == null) return Tab.INVALID_TAB_ID;

        // TODO(b/537362347): Update method to remove usage of getCurrentActingTab() when
        // refactoring for multi-task.
        ActorTask task = mActorService.getCurrentActiveTask();
        if (task == null) return Tab.INVALID_TAB_ID;

        Set<Integer> tabIds = task.getLastActedTabs();
        if (!tabIds.isEmpty()) {
            return tabIds.iterator().next();
        }
        return Tab.INVALID_TAB_ID;
    }
}
