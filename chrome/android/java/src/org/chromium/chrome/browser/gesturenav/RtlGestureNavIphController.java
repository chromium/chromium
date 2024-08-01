// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
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

    private static final String UNHANDLED_GESTURE_THRESHOLD_PARAM = "x_unhandled_gesture_threshold";
    private static final int DEFAULT_UNHANDLED_GESTURE_THRESHOLD = 2;
    private static final String TRIGGER_METHOD_PARAM = "x_trigger";
    private static final String TRIGGERED_BY_NON_EMPTY_STACK = "non-empty-stack";

    private @Nullable RtlGestureNavTabObserver mRtlGestureNavTabObserver;
    private final ActivityTabProvider mActivityTabProvider;
    private final Supplier<Profile> mProfileSupplier;
    private int mUnhandledGestureCount;
    private final int mUnhandledGestureThreshold;

    /**
     * @param activityTabProvider The tab provider of providing the current tab.
     */
    public RtlGestureNavIphController(
            ActivityTabProvider activityTabProvider, Supplier<Profile> profileSupplier) {
        mActivityTabProvider = activityTabProvider;
        mProfileSupplier = profileSupplier;
        mUnhandledGestureThreshold =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        FeatureConstants.IPH_RTL_GESTURE_NAVIGATION,
                        UNHANDLED_GESTURE_THRESHOLD_PARAM,
                        DEFAULT_UNHANDLED_GESTURE_THRESHOLD);
        if (shouldShowOnNonEmptyStack() && wouldShowIph()) {
            mRtlGestureNavTabObserver = new RtlGestureNavTabObserver(activityTabProvider);
        }
    }

    public void onGestureUnhandled() {
        if (shouldShowOnNonEmptyStack()) return;
        if (mActivityTabProvider.get() == null) return;
        mUnhandledGestureCount++;
        if (mUnhandledGestureCount >= mUnhandledGestureThreshold) {
            Tracker tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
            if (tracker.shouldTriggerHelpUI(FeatureConstants.IPH_RTL_GESTURE_NAVIGATION)) {
                show();
                mUnhandledGestureCount = 0;
            }
        }
    }

    public void onGestureHandled() {
        mUnhandledGestureCount = 0;
    }

    @Override
    public void destroy() {
        if (mRtlGestureNavTabObserver != null) {
            mRtlGestureNavTabObserver.destroy();
        }
    }

    private void onNavStateChanged(@Nullable Tab tab) {
        if (tab == null) return;
        assert shouldShowOnNonEmptyStack();
        if (tab.canGoBack() || tab.canGoForward()) {
            Tracker tracker = TrackerFactory.getTrackerForProfile(tab.getProfile());
            if (tracker.shouldTriggerHelpUI(FeatureConstants.IPH_RTL_GESTURE_NAVIGATION)) {
                show();
                mRtlGestureNavTabObserver.destroy();
                mRtlGestureNavTabObserver = null;
            }
        }
    }

    private void show() {
        Tab tab = mActivityTabProvider.get();
        RtlGestureNavIphDialog dialog =
                new RtlGestureNavIphDialog(
                        tab.getContext(),
                        tab.getWindowAndroid().getModalDialogManager(),
                        () -> {
                            Tracker tracker =
                                    TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
                            tracker.dismissed(FeatureConstants.IPH_RTL_GESTURE_NAVIGATION);
                        });
        dialog.setParentView((ViewGroup) tab.getView());
        dialog.show();
        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        tracker.notifyEvent(EventConstants.RTL_GESTURE_NAVIGATION_DIALOG_SHOW);
    }

    private boolean wouldShowIph() {
        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        return tracker.wouldTriggerHelpUI(FeatureConstants.IPH_RTL_GESTURE_NAVIGATION);
    }

    @VisibleForTesting
    boolean shouldShowOnNonEmptyStack() {
        return ChromeFeatureList.getFieldTrialParamByFeature(
                        FeatureConstants.IPH_RTL_GESTURE_NAVIGATION, TRIGGER_METHOD_PARAM)
                .equals(TRIGGERED_BY_NON_EMPTY_STACK);
    }
}
