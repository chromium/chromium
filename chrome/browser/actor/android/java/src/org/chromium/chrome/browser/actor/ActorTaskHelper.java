// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Activity;
import android.view.WindowManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.function.Supplier;

/** Helper class that keeps the screen on while an Actor task is active. */
@NullMarked
public class ActorTaskHelper implements ActorKeyedService.Observer {
    private final Activity mActivity;
    private final Supplier<Profile> mProfileSupplier;
    private @Nullable ActorKeyedService mActorService;
    private boolean mKeepScreenOn;

    /**
     * @param activity The {@link Activity} to manage flags for.
     * @param profileSupplier Supplier for the current {@link Profile}.
     */
    public ActorTaskHelper(Activity activity, Supplier<Profile> profileSupplier) {
        mActivity = activity;
        mProfileSupplier = profileSupplier;
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
        ActorKeyedService service = maybeGetActorService();
        if (service == null) return false;
        for (ActorTask task : service.getActiveTasks()) {
            if (task.getState() == ActorTaskState.CREATED
                    || task.getState() == ActorTaskState.ACTING
                    || task.getState() == ActorTaskState.REFLECTING) {
                return true;
            }
        }
        return false;
    }

    private @Nullable ActorKeyedService maybeGetActorService() {
        if (mActorService != null) return mActorService;
        Profile profile = mProfileSupplier.get();
        if (profile == null) return null;
        mActorService = ActorKeyedServiceFactory.getForProfile(profile);
        if (mActorService != null) {
            mActorService.addObserver(this);
        }
        return mActorService;
    }

    /** Cleans up the helper, removing observers and clearing flags. */
    public void destroy() {
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
