// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.support.annotation.IntDef;

import com.google.android.libraries.feed.api.lifecycle.AppLifecycleListener;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.signin.SigninManager;

import java.lang.ref.WeakReference;
import java.util.List;

/**
 * Aggregation point for application lifecycle events that the Feed cares about. Events that
 * originate in Java flow directly to FeedAppLifecycle, while native-originating events arrive
 * via {@link FeedLifecycleBridge}.
 */
public class FeedAppLifecycle
        implements SigninManager.SignInStateObserver, ApplicationStatus.ActivityStateListener {
    @IntDef({AppLifecycleEvent.ENTER_FOREGROUND, AppLifecycleEvent.ENTER_BACKGROUND,
            AppLifecycleEvent.CLEAR_ALL, AppLifecycleEvent.INITIALIZE,
            AppLifecycleEvent.NUM_ENTRIES})

    // Intdef used to assign each event a number for metrics logging purposes. This maps directly to
    // the AppLifecycleEvent enum defined in tools/metrics/enums.xml
    public @interface AppLifecycleEvent {
        int ENTER_FOREGROUND = 0;
        int ENTER_BACKGROUND = 1;
        int CLEAR_ALL = 2;
        int INITIALIZE = 3;
        int NUM_ENTRIES = 4;
    }

    private AppLifecycleListener mAppLifecycleListener;
    private FeedLifecycleBridge mLifecycleBridge;
    private FeedScheduler mFeedScheduler;

    private int mTabbedActivityCount;
    private boolean mInitializeCalled;

    /**
     * Create a FeedAppLifecycle instance. In normal use, this should only be called by {@link
     * FeedAppLifecycleFactory}.
     * @param appLifecycleListener The Feed-side instance of the {@link AppLifecycleListener}
     *        interface that we will call into.
     * @param lifecycleBridge FeedLifecycleBridge JNI bridge over which native lifecycle events are
     *        delivered.
     */
    public FeedAppLifecycle(AppLifecycleListener appLifecycleListener,
            FeedLifecycleBridge lifecycleBridge, FeedScheduler feedScheduler) {
        mAppLifecycleListener = appLifecycleListener;
        mLifecycleBridge = lifecycleBridge;
        mFeedScheduler = feedScheduler;

        int resumedActivityCount = 0;
        List<WeakReference<Activity>> activities = ApplicationStatus.getRunningActivities();
        for (final WeakReference<Activity> ref : activities) {
            final Activity activity = ref.get();
            if (activity != null && activity instanceof ChromeTabbedActivity) {
                @ActivityState
                int activityState = ApplicationStatus.getStateForActivity(activity);
                if (activityState != ActivityState.STOPPED) {
                    ++mTabbedActivityCount;
                }
                if (activityState == ActivityState.RESUMED) {
                    ++resumedActivityCount;
                }
            }
        }

        if (mTabbedActivityCount > 0) {
            onEnterForeground();
        }
        // The scheduler cares about Chrome entering the visual foreground, which corresponds to the
        // RESUMED state. This state is entered regardless of whether or not the Activity was
        // previously paused.
        if (resumedActivityCount > 0) {
            mFeedScheduler.onForegrounded();
        }

        ApplicationStatus.registerStateListenerForAllActivities(this);
        SigninManager.get().addSignInStateObserver(this);
    }

    /**
     * This is called when an NTP is shown.
     */
    public void onNTPOpened() {
        initialize();
    }

    /**
     * This is called when the user has deleted some non-trivial number of history entries.
     * We call onClearAll to avoid presenting personalized suggestions based on deleted history.
     */
    public void onHistoryDeleted() {
        onClearAll();
    }

    /**
     * This is called when cached browsing data is cleared. We call onClearAll so that the
     * Feed deletes its cached browsing data.
     */
    public void onCachedDataCleared() {
        onClearAll();
    }

    /**
     * Unregisters listeners and cleans up any native resources held by FeedAppLifecycle.
     */
    public void destroy() {
        SigninManager.get().removeSignInStateObserver(this);
        ApplicationStatus.unregisterActivityStateListener(this);
        mLifecycleBridge.destroy();
        mLifecycleBridge = null;
        mAppLifecycleListener = null;
        mFeedScheduler = null;
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        // We only care about ChromeTabbedActivity since no other type of activity could potentially
        // show the Feed.
        if (activity != null && activity instanceof ChromeTabbedActivity) {
            switch (newState) {
                case ActivityState.STOPPED:
                    --mTabbedActivityCount;
                    if (mTabbedActivityCount == 0) {
                        onEnterBackground();
                    }
                    break;
                case ActivityState.STARTED:
                    ++mTabbedActivityCount;
                    if (mTabbedActivityCount == 1) {
                        onEnterForeground();
                    }
                    break;
                case ActivityState.RESUMED:
                    mFeedScheduler.onForegrounded();
                    break;
            }
        }
    }

    @Override
    public void onSignedIn() {
        onClearAll();
    }

    @Override
    public void onSignedOut() {
        onClearAll();
    }

    private void onEnterForeground() {
        reportEvent(AppLifecycleEvent.ENTER_FOREGROUND);
        mAppLifecycleListener.onEnterForeground();
    }

    private void onEnterBackground() {
        reportEvent(AppLifecycleEvent.ENTER_BACKGROUND);
        mAppLifecycleListener.onEnterBackground();
    }

    private void onClearAll() {
        reportEvent(AppLifecycleEvent.CLEAR_ALL);
        mAppLifecycleListener.onClearAll();
    }

    private void initialize() {
        if (!mInitializeCalled) {
            reportEvent(AppLifecycleEvent.INITIALIZE);
            mAppLifecycleListener.initialize();
            mInitializeCalled = true;
        }
    }

    private void reportEvent(@AppLifecycleEvent int event) {
        RecordHistogram.recordEnumeratedHistogram("ContentSuggestions.Feed.AppLifecycle.Events",
                event, AppLifecycleEvent.NUM_ENTRIES);
    }
}
