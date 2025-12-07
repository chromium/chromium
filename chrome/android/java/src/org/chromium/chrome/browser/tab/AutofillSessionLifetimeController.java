// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.app.Activity;
import android.view.autofill.AutofillManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.content_public.browser.NavigationHandle;

/**
 * Handles the lifetime of the current Autofill session.
 *
 * <p>The Android Autofill service only tracks a limited number (default: 10) of view sets with an
 * open fill request per Autofill session. In order to keep Autofill triggering, the session has to
 * be finished (cancelled or committed) periodically.
 *
 * <p>Additionally, Autofill sessions that clearly should not trigger a save flow can be cancelled
 * in order to reduce save UI false positives. The Autofill service triggers save when all Autofill-
 * relevant virtual views become invisible, so care must be taken to cancel the session beforehand.
 *
 * <pre>
 * Autofill sessions are cancelled:
 * 1. when the domain part of the UrlBar content changes:
 *    In this case the session is cancelled by the Android Autofill service's compat mode.
 * 2. right before the tab is hidden, unless Chrome itself is being stopped:
 *    Ensures that no save UI is shown when the user switches to a different tab or the tab
 *    switcher. By cancelling the session in onInteractabilityChanged, we catch both cases at a
 *    point where tab contents are not yet hidden. Since some third-party Autofill services use
 *    fullscreen authentication flows before they fill, the session must be preserved through
 *    the main activity's lifecycle events.
 * 3. when browser-initiated navigation occurs:
 *    As opposed to renderer-initiated navigation (e.g., submitting a form), navigation initiated by
 *    browser controls should never trigger save UI. In order to cancel the session before web
 *    content views become invisible, we have to use onDidStartNavigationInPrimaryMainFrame rather
 *    than one of the later events.
 * </pre>
 */
@NullMarked
public class AutofillSessionLifetimeController implements DestroyObserver {
    private final Activity mActivity;
    private final ActivityTabProvider.ActivityTabTabObserver mActivityTabObserver;

    public AutofillSessionLifetimeController(
            Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            ActivityTabProvider activityTabProvider) {
        mActivity = activity;
        mActivityTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(activityTabProvider) {
                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigationHandle) {
                        if (!navigationHandle.isRendererInitiated()) {
                            cancel();
                        }
                    }

                    @Override
                    public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                        // While onInteractabilityChanged is called in ChromeActivity.onStop(), the
                        // session must remain active to allow Autofill services' fullscreen
                        // authentication flows to succeed.
                        boolean isStopped =
                                lifecycleDispatcher.getCurrentActivityState()
                                        == ActivityLifecycleDispatcher.ActivityState
                                                .STOPPED_WITH_NATIVE;
                        if (!isInteractable && !isStopped) {
                            cancel();
                        }
                    }
                };
        lifecycleDispatcher.register(this);
    }

    private void cancel() {
        // The AutofillManager has to be retrieved from an activity context.
        // https://cs.android.com/android/platform/superproject/+/master:frameworks/base/core/java/android/app/Application.java;l=624;drc=5d123b67756dffcfdebdb936ab2de2b29c799321
        AutofillManager afm = mActivity.getSystemService(AutofillManager.class);
        if (afm != null) {
            afm.cancel();
        }
    }

    // DestroyObserver
    @Override
    public void onDestroy() {
        mActivityTabObserver.destroy();
    }
}
