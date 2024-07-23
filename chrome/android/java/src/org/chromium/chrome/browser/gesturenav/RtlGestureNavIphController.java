// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerState;
import org.chromium.content_public.browser.NavigationHandle;

/**
 * A controller triggering the Iph Dialog when conditions are satisfied.
 *
 * <p>First arm: when both back stack and forward stack are not empty.
 *
 * <p>Second arm: when user tries to navigate back/forward but there is nothing in the corresponding
 * history stack; i.e. fail to navigate back/forward because of empty stack.
 */
public class RtlGestureNavIphController implements Destroyable {

    private class RtlGestureNavTabObserver extends ActivityTabTabObserver {
        public RtlGestureNavTabObserver(ActivityTabProvider tabProvider) {
            super(tabProvider);
        }

        @Override
        public void onDidFinishNavigationInPrimaryMainFrame(
                Tab tab, NavigationHandle navigationHandle) {
            onNavStateChanged(tab);
        }
    }

    private @NonNull RtlGestureNavTabObserver mRtlGestureNavTabObserver;

    /**
     * @param activityTabProvider The tab provider of providing the current tab.
     */
    public RtlGestureNavIphController(ActivityTabProvider activityTabProvider) {
        mRtlGestureNavTabObserver = new RtlGestureNavTabObserver(activityTabProvider);
    }

    @Override
    public void destroy() {
        mRtlGestureNavTabObserver.destroy();
    }

    private void onNavStateChanged(@Nullable Tab tab) {
        if (tab == null) return;

        if (tab.canGoBack() || tab.canGoForward()) {
            Tracker tracker = TrackerFactory.getTrackerForProfile(tab.getProfile());
            if (tracker.shouldTriggerHelpUI(FeatureConstants.IPH_RTL_GESTURE_NAVIGATION)) {
                show();
                mRtlGestureNavTabObserver.destroy();
            } else if (tracker.getTriggerState(FeatureConstants.IPH_RTL_GESTURE_NAVIGATION)
                    == TriggerState.HAS_BEEN_DISPLAYED) {
                mRtlGestureNavTabObserver.destroy();
            }
        }
    }

    // TODO(https://crbug.com/336347851): implement dialog
    private void show() {}
}
