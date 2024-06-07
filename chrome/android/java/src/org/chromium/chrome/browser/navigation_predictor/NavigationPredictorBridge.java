// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.navigation_predictor;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.function.BooleanSupplier;

/** Exposes methods to report tabs moving to foreground/background. */
public class NavigationPredictorBridge implements PauseResumeWithNativeObserver {
    private final Profile mProfile;
    private final BooleanSupplier mIsWarmStartSupplier;

    /**
     * Constructs a new {@link NavigationPredictorBridge} instance.
     *
     * @param profile The Profile that will utilize the navigation predictions.
     * @param activityLifecycleDispatcher The activity associated with the navigations.
     * @param isWarmStartSupplier Supplies whether the current run of the activity is associated
     *     with a warm start.
     */
    public NavigationPredictorBridge(
            Profile profile,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            BooleanSupplier isWarmStartSupplier) {
        mProfile = profile;
        mIsWarmStartSupplier = isWarmStartSupplier;
        activityLifecycleDispatcher.register(this);
    }

    @Override
    public void onResumeWithNative() {
        if (mIsWarmStartSupplier.getAsBoolean()) {
            NavigationPredictorBridgeJni.get().onActivityWarmResumed(mProfile);
        } else {
            NavigationPredictorBridgeJni.get().onColdStart(mProfile);
        }
    }

    @Override
    public void onPauseWithNative() {
        NavigationPredictorBridgeJni.get().onPause(mProfile);
    }

    @NativeMethods
    interface Natives {
        void onActivityWarmResumed(@JniType("Profile*") Profile profile);

        void onColdStart(@JniType("Profile*") Profile profile);

        void onPause(@JniType("Profile*") Profile profile);
    }
}
