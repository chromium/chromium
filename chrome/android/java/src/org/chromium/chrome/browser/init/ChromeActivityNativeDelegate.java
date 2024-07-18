// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Intent;

import androidx.annotation.Nullable;

/**
 * An activity level delegate to handle native initialization and activity lifecycle related tasks
 * that depend on native code. The lifecycle callbacks defined here are called synchronously with
 * their counterparts in Activity only if the native libraries have been loaded. If the libraries
 * have not been loaded yet, the calls in ChromeActivityNativeDelegate will be in the right order
 * among themselves, but can be deferred and out of sync wrt to Activity calls.
 */
public interface ChromeActivityNativeDelegate {
    /**
     * Carry out native initialization related tasks and any other java tasks that can be done async
     * with native initialization.
     */
    void onCreateWithNative();

    /** Carry out native code dependent tasks that should happen during on Activity.onStart(). */
    void onStartWithNative();

    /** Carry out native code dependent tasks that should happen during on Activity.onResume(). */
    void onResumeWithNative();

    /**
     * Carry out native code dependent tasks that should happen during
     * Activity.onTopResumedActivityChanged(isTopResumedActivity).
     *
     * <p>If the last lifecycle event before native initialization arrives with
     * isTopResumedActivity=false, it cancels the previously pending top resumed state resulting in
     * no callback at native initialization time.
     *
     * @param isTopResumedActivity is taken directly from the activity lifecycle callback; only the
     *     last value arriving before native initialization is passed, previous ones are lost.
     */
    void onTopResumedActivityChangedWithNative(boolean isTopResumedActivity);

    /** Carry out native code dependent tasks that should happen during on Activity.onPause(). */
    void onPauseWithNative();

    /** Carry out native code dependent tasks that should happen during on Activity.onStop(). */
    void onStopWithNative();

    /**
     * @return Whether the activity linked to the delegate has been destroyed or is finishing. The
     *         majority of clients should prefer the method in {@link ActivityUtils}.
     */
    boolean isActivityFinishingOrDestroyed();

    /**
     * Carry out native code dependent tasks that relate to processing a new intent coming to
     * FragmentActivity.onNewIntent().
     */
    void onNewIntentWithNative(Intent intent);

    /**
     * @return The Intent that launched the activity.
     */
    Intent getInitialIntent();

    /**
     * Carry out native code dependent tasks that relate to processing an activity result coming to
     * Activity.onActivityResult().
     * @param requestCode The request code of the response.
     * @param resultCode  The result code of the response.
     * @param data        The intent data of the response.
     * @return            Whether or not the result was handled
     */
    boolean onActivityResultWithNative(int requestCode, int resultCode, Intent data);

    /**
     * Called when any failure about the initialization occurs.
     * @param failureCause The Exception from the original failure.
     */
    void onStartupFailure(@Nullable Exception failureCause);
}
