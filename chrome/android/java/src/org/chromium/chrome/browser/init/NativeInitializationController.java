// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Intent;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.flags.ChromeSwitches;

import java.util.ArrayList;
import java.util.List;

/**
 * This class controls the different asynchronous states during our initialization:
 * 1. During startBackgroundTasks(), we'll kick off loading the library and yield the call stack.
 * 2. We may receive a onStart() / onStop() call any point after that, whether or not
 *    the library has been loaded.
 */
class NativeInitializationController {
    private static final String TAG = "NIController";

    private final ChromeActivityNativeDelegate mActivityDelegate;

    private boolean mOnStartPending;
    private boolean mOnResumePending;
    private List<Intent> mPendingNewIntents;
    private List<ActivityResult> mPendingActivityResults;

    private Boolean mBackgroundTasksComplete;
    private boolean mHasDoneFirstDraw;
    private boolean mHasSignaledLibraryLoaded;
    private boolean mInitializationComplete;
    private boolean mOnTopResumedPending;

    /**
     * This class encapsulates a call to onActivityResult that has to be deferred because the native
     * library is not yet loaded.
     */
    static class ActivityResult {
        public final int requestCode;
        public final int resultCode;
        public final Intent data;

        public ActivityResult(int requestCode, int resultCode, Intent data) {
            this.requestCode = requestCode;
            this.resultCode = resultCode;
            this.data = data;
        }
    }

    /**
     * Create the NativeInitializationController using the main loop and the application context.
     * It will be linked back to the activity via the given delegate.
     * @param activityDelegate The activity delegate for the owning activity.
     */
    public NativeInitializationController(ChromeActivityNativeDelegate activityDelegate) {
        mActivityDelegate = activityDelegate;
    }

    /**
     * Start loading the native library in the background. This kicks off the native initialization
     * process.
     *
     * @param allocateChildConnection Whether a spare child connection should be allocated. Set to
     *                                false if you know that no new renderer is needed.
     */
    public void startBackgroundTasks(final boolean allocateChildConnection) {
        ThreadUtils.assertOnUiThread();
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION)) {
            Log.i(TAG, "Exit early and start Chrome without loading native library!");
            return;
        }

        // This is a fairly low cost way to check if fetching the variations seed is needed. It can
        // produces false positives, but that's okay. There's a later mechanism that checks a
        // dedicated durable field to make sure the actual network request is only made once.
        boolean fetchVariationsSeed =
                FirstRunFlowSequencer.checkIfFirstRunIsNecessary(
                        false, mActivityDelegate.getInitialIntent());

        mBackgroundTasksComplete = false;
        new AsyncInitTaskRunner() {

            @Override
            protected void onSuccess() {
                ThreadUtils.assertOnUiThread();

                mBackgroundTasksComplete = true;
                signalNativeLibraryLoadedIfReady();
            }

            @Override
            protected void onFailure(Exception failureCause) {
                // Initialization has failed, call onStartup failure to abandon the activity.
                // This is not expected to return, so there is no need to set
                // mBackgroundTasksComplete or do any other tidying up.
                mActivityDelegate.onStartupFailure(failureCause);
            }
        }.startBackgroundTasks(allocateChildConnection, fetchVariationsSeed);
    }

    private void signalNativeLibraryLoadedIfReady() {
        ThreadUtils.assertOnUiThread();

        // Called on UI thread when any of the booleans below have changed.
        if (mHasDoneFirstDraw && (mBackgroundTasksComplete != null && mBackgroundTasksComplete)) {
            // This block should only be hit once.
            assert !mHasSignaledLibraryLoaded;
            mHasSignaledLibraryLoaded = true;

            if (mActivityDelegate.isActivityFinishingOrDestroyed()) return;
            mActivityDelegate.onCreateWithNative();
        }
    }

    /**
     * Called when the current activity has finished its first draw pass. This and the library
     * load has to be completed to start the chromium browser process.
     */
    public void firstDrawComplete() {
        mHasDoneFirstDraw = true;
        signalNativeLibraryLoadedIfReady();
    }

    /** Called when native initialization for an activity has been finished. */
    public void onNativeInitializationComplete() {
        // Callback when we finished with ChromeActivityNativeDelegate.onCreateWithNative tasks
        mInitializationComplete = true;

        if (mOnStartPending) {
            mOnStartPending = false;
            startNowAndProcessPendingItems();
        }

        if (mOnResumePending) {
            mOnResumePending = false;
            onResume();
        }

        if (mOnTopResumedPending) {
            mOnTopResumedPending = false;
            onTopResumedActivityChanged(true);
        }
    }

    /** Called when an activity gets an onStart call and is done with java only tasks. */
    public void onStart() {
        if (mInitializationComplete) {
            startNowAndProcessPendingItems();
        } else {
            mOnStartPending = true;
        }
    }

    /** Called when an activity gets an onResume call and is done with java only tasks. */
    public void onResume() {
        if (mInitializationComplete) {
            mActivityDelegate.onResumeWithNative();
        } else {
            mOnResumePending = true;
        }
    }

    /**
     * Called when activity gets or loses the top resumed position in the system.
     *
     * @param isTopResumedActivity {@code true} if it's the topmost resumed activity in the system,
     *     {@code false} otherwise. A call with this as {@code true} will always be followed by
     *     another one with {@code false}.
     */
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        if (mInitializationComplete) {
            mActivityDelegate.onTopResumedActivityChangedWithNative(isTopResumedActivity);
        } else {
            mOnTopResumedPending = isTopResumedActivity;
        }
    }

    /** Called when an activity gets an onPause call and is done with java only tasks. */
    public void onPause() {
        mOnResumePending = false; // Clear the delayed resume if a pause happens first.
        if (mInitializationComplete) mActivityDelegate.onPauseWithNative();
    }

    /** Called when an activity gets an onStop call and is done with java only tasks. */
    public void onStop() {
        mOnStartPending = false; // Clear the delayed start if a stop happens first.
        if (!mInitializationComplete) return;
        mActivityDelegate.onStopWithNative();
    }

    /**
     * Called when an activity gets an onNewIntent call and is done with java only tasks.
     * @param intent The intent that has arrived to the activity linked to the given delegate.
     */
    public void onNewIntent(Intent intent) {
        if (mInitializationComplete) {
            mActivityDelegate.onNewIntentWithNative(intent);
        } else {
            if (mPendingNewIntents == null) mPendingNewIntents = new ArrayList<>(1);
            mPendingNewIntents.add(intent);
        }
    }

    /**
     * This is the Android onActivityResult callback deferred, if necessary,
     * to when the native library has loaded.
     * @param requestCode The request code for the ActivityResult.
     * @param resultCode The result code for the ActivityResult.
     * @param data The intent that has been sent with the ActivityResult.
     */
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (mInitializationComplete) {
            mActivityDelegate.onActivityResultWithNative(requestCode, resultCode, data);
        } else {
            if (mPendingActivityResults == null) {
                mPendingActivityResults = new ArrayList<>(1);
            }
            mPendingActivityResults.add(new ActivityResult(requestCode, resultCode, data));
        }
    }

    private void startNowAndProcessPendingItems() {
        try (TraceEvent te = TraceEvent.scoped("startNowAndProcessPendingItems")) {
            // onNewIntent and onActivityResult are called only when the activity is paused.
            // To match the non-deferred behavior, onStart should be called before any processing
            // of pending intents and activity results.
            // Note that if we needed ChromeActivityNativeDelegate.onResumeWithNative(), the pending
            // intents and activity results processing should have happened in the corresponding
            // resumeNowAndProcessPendingItems, just before the call to
            // ChromeActivityNativeDelegate.onResumeWithNative().
            mActivityDelegate.onStartWithNative();

            if (mPendingNewIntents != null) {
                for (Intent intent : mPendingNewIntents) {
                    mActivityDelegate.onNewIntentWithNative(intent);
                }
                mPendingNewIntents = null;
            }

            if (mPendingActivityResults != null) {
                ActivityResult activityResult;
                for (int i = 0; i < mPendingActivityResults.size(); i++) {
                    activityResult = mPendingActivityResults.get(i);
                    mActivityDelegate.onActivityResultWithNative(
                            activityResult.requestCode,
                            activityResult.resultCode,
                            activityResult.data);
                }
                mPendingActivityResults = null;
            }
        }
    }
}
