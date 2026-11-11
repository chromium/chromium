// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.actor.ActorUtils;
import org.chromium.chrome.browser.actor.foreground_service.ActorForegroundServiceManager;

/**
 * JNI bridge for managing the system notification shown when a remote Glic experimental triggering
 * opt-in request arrives while Chrome is in the background.
 */
@JNINamespace("glic")
@NullMarked
class GlicExperimentalOptInBridge {
    private static boolean sIsShowing;

    private static final ApplicationStatus.ActivityStateListener sActivityStateListener =
            (activity, newState) -> {
                if (!sIsShowing) {
                    return;
                }
                if (newState == ActivityState.STARTED || newState == ActivityState.RESUMED) {
                    GlicExperimentalOptInBridgeJni.get().onActivityStartedOrResumed();
                }
            };

    private GlicExperimentalOptInBridge() {}

    /**
     * Pins the background opt-in notification to {@link ActorForegroundServiceManager} so the
     * foreground service stays alive while waiting for the user.
     *
     * @return True if the notification was requested, or false if notifications are disabled or the
     *     foreground service manager is unavailable.
     */
    @CalledByNative
    static boolean show() {
        if (sIsShowing) {
            return true;
        }
        if (!ActorUtils.areActorNotificationsEnabled()) {
            return false;
        }
        ActorForegroundServiceManager fgsManager = ActorForegroundServiceManager.getInstance();
        if (fgsManager == null) {
            return false;
        }

        sIsShowing = true;
        ApplicationStatus.registerStateListenerForAllActivities(sActivityStateListener);
        fgsManager.pinOptInNotification(GlicExperimentalOptInBridge::onOptInCancelled);
        return sIsShowing;
    }

    /** Dismisses the background opt-in notification and unpins it from the foreground service. */
    @CalledByNative
    static void hide() {
        if (!sIsShowing) {
            return;
        }
        resetShowingState();
        ActorForegroundServiceManager fgsManager = ActorForegroundServiceManager.getInstance();
        if (fgsManager != null) {
            fgsManager.unpinOptInNotification();
        }
    }

    /**
     * Handles cancellation of the background opt-in notification (e.g. when the user taps "No
     * thanks", an active task preempts it, or the foreground service is destroyed) by clearing
     * showing state and notifying native to fail the deferred opt-in request and stop the timeout
     * timer.
     */
    private static void onOptInCancelled() {
        if (!sIsShowing) {
            return;
        }
        resetShowingState();
        GlicExperimentalOptInBridgeJni.get().onOptInCancelled();
    }

    /** Returns whether the background opt-in notification is currently showing. */
    static boolean isShowingForTesting() {
        return sIsShowing;
    }

    /** Resets the notification showing state for testing. */
    static void resetForTesting() {
        resetShowingState();
    }

    private static void resetShowingState() {
        sIsShowing = false;
        ApplicationStatus.unregisterActivityStateListener(sActivityStateListener);
    }

    @NativeMethods
    interface Natives {
        void onOptInCancelled();

        void onActivityStartedOrResumed();
    }
}
