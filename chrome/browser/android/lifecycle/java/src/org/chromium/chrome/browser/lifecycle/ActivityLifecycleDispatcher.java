// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Manages registration of {@link LifecycleObserver} instances. */
public interface ActivityLifecycleDispatcher {
    /** A set of states that represent the last state change of an Activity. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        ActivityState.CREATED_WITH_NATIVE,
        ActivityState.STARTED_WITH_NATIVE,
        ActivityState.RESUMED_WITH_NATIVE,
        ActivityState.PAUSED_WITH_NATIVE,
        ActivityState.STOPPED_WITH_NATIVE,
        ActivityState.DESTROYED
    })
    @interface ActivityState {
        /** Called when the activity is created, provided that native is initialized. */
        int CREATED_WITH_NATIVE = 1;

        /**
         * Called when the activity is started, provided that native is initialized.
         * If native is not initialized at that point, the call is postponed until it is.
         */
        int STARTED_WITH_NATIVE = 2;

        /** Called when the activity is resumed, provided that native is initialized. */
        int RESUMED_WITH_NATIVE = 3;

        /** Called when the activity is paused, provided that native is initialized. */
        int PAUSED_WITH_NATIVE = 4;

        /** Called when the activity is stopped, provided that native is initialized. */
        int STOPPED_WITH_NATIVE = 5;

        /**
         * Represents Activity#onDestroy().
         * This is also used when the state of an Activity is unknown.
         */
        int DESTROYED = 6;
    }

    /**
     * Registers an observer.
     * @param observer must implement one or several observer interfaces in
     * {@link org.chromium.chrome.browser.lifecycle} in order to receive corresponding events.
     */
    void register(LifecycleObserver observer);

    /** Unregisters an observer. */
    void unregister(LifecycleObserver observer);

    /**
     * @return The current {@link ActivityState} for the activity associated with this dispatcher.
     */
    @ActivityState
    int getCurrentActivityState();

    /** @return Whether native initialization is complete. */
    boolean isNativeInitializationFinished();

    /**
     * @return Whether the current activity associated with this dispatcher has been destroyed or is
     *         in the process of finishing.
     */
    boolean isActivityFinishingOrDestroyed();
}
