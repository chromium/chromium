// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Activity;
import android.view.WindowManager;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/** Helper class that keeps the screen on while an Actor task is active. */
@NullMarked
public class ActorTaskHelper implements ActorKeyedService.Observer {
    private final Activity mActivity;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileObserver = (p) -> updateKeepScreenOn();
    private @Nullable ActorKeyedService mActorService;
    private boolean mKeepScreenOn;

    /**
     * @param activity The {@link Activity} to manage flags for.
     * @param profileSupplier Supplier for the current {@link Profile}.
     */
    public ActorTaskHelper(
            Activity activity, MonotonicObservableSupplier<Profile> profileSupplier) {
        mActivity = activity;
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addSyncObserverAndCallIfNonNull(mProfileObserver);
    }

    @Override
    public void onTaskStateChanged(int taskId, @ActorTaskState int newState) {
        updateKeepScreenOn();
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
        forEachActiveTask(task -> hasActiveTask[0] = true);
        return hasActiveTask[0];
    }

    /** Pauses any Actor tasks if they are in an active state. */
    public void onStop() {
        forEachActiveTask(task -> task.pause());
    }

    private void forEachActiveTask(Callback<ActorTask> callback) {
        ActorKeyedService service = maybeGetActorService();
        if (service == null) return;
        for (ActorTask task : service.getActiveTasks()) {
            if (ActorUtils.isRunningState(task.getState())) {
                callback.onResult(task);
            }
        }
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

    /** Cleans up the helper, removing observers and clearing flags. */
    public void destroy() {
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
}
