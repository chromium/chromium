// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;

/** An ActivityLifecycleCallbacks that routes all methods to a single onStateChanged(). */
public abstract class ActivityLifecycleCallbacksAdapter
        implements Application.ActivityLifecycleCallbacks {
    public abstract void onStateChanged(Activity activity, @ActivityState int newState);

    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
        onStateChanged(activity, ActivityState.CREATED);
    }

    @Override
    public void onActivityDestroyed(Activity activity) {
        onStateChanged(activity, ActivityState.DESTROYED);
    }

    @Override
    public void onActivityPaused(Activity activity) {
        onStateChanged(activity, ActivityState.PAUSED);
    }

    @Override
    public void onActivityResumed(Activity activity) {
        onStateChanged(activity, ActivityState.RESUMED);
    }

    @Override
    public void onActivityStarted(Activity activity) {
        onStateChanged(activity, ActivityState.STARTED);
    }

    @Override
    public void onActivityStopped(Activity activity) {
        onStateChanged(activity, ActivityState.STOPPED);
    }

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle bundle) {}
}
