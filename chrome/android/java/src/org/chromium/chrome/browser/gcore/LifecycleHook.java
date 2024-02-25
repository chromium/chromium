// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gcore;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.util.HashSet;
import java.util.Set;

/**
 * Listens to application state changes. It is created lazily when we want to register a
 * {@link GoogleApiClientHelper}.
 * Thread safe.
 */
class LifecycleHook implements ApplicationStateListener {
    private static LifecycleHook sInstance;
    private static final Object sInstanceLock = new Object();
    private static final String TAG = "GCore";

    public static LifecycleHook getInstance() {
        synchronized (sInstanceLock) {
            if (sInstance == null) sInstance = new LifecycleHook();
            return sInstance;
        }
    }

    /**
     * Reset static singletons.
     * This is needed for JUnit tests as statics are not reset between runs and previous states can
     * make other tests fail. It is not needed in instrumentation tests (and will be removed by
     * Proguard in release builds) since the application lifecycle will naturally do the work.
     */
    public static void destroyInstanceForJUnitTests() {
        LifecycleHook hook;
        synchronized (sInstanceLock) {
            if (sInstance == null) return;
            hook = sInstance;
            sInstance = null;
        }
        ApplicationStatus.unregisterApplicationStateListener(hook);
    }

    private final Set<GoogleApiClientHelper> mClientHelpers;
    private boolean mIsApplicationVisible;

    private LifecycleHook() {
        mClientHelpers = new HashSet<GoogleApiClientHelper>();
        mIsApplicationVisible = ApplicationStatus.hasVisibleActivities();
        ApplicationStatus.registerApplicationStateListener(this);
        Log.d(TAG, "lifecycle hook registered.");
    }

    public void registerClientHelper(GoogleApiClientHelper clientHelper) {
        synchronized (mClientHelpers) {
            mClientHelpers.add(clientHelper);
        }
    }

    public void unregisterClientHelper(GoogleApiClientHelper clientHelper) {
        synchronized (mClientHelpers) {
            mClientHelpers.remove(clientHelper);
        }
    }

    @Override
    public void onApplicationStateChange(int newState) {
        Log.d(TAG, "onApplicationStateChange");
        ThreadUtils.assertOnUiThread();

        boolean newVisibility = ApplicationStatus.hasVisibleActivities();
        if (mIsApplicationVisible == newVisibility) return;

        Log.d(
                TAG,
                "Application visibilty changed to %s. Updating state of %d client(s).",
                newVisibility,
                mClientHelpers.size());

        mIsApplicationVisible = newVisibility;

        synchronized (mClientHelpers) {
            for (GoogleApiClientHelper clientHelper : mClientHelpers) {
                if (mIsApplicationVisible) clientHelper.restoreConnectedState();
                else clientHelper.scheduleDisconnection();
            }
        }
    }
}
