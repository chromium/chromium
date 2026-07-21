// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscription_eligibility;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Java interface for the SubscriptionEligibilityService. Observes AI tier changes and surfaces them
 * to Android UI components.
 */
@NullMarked
public class SubscriptionEligibilityService {
    /** Observer for AI subscription tier changes. */
    public interface Observer {
        void onAiSubscriptionTierChanged();
    }

    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private long mNativeBridge;

    /** Constructs the service and creates the corresponding C++ bridge. */
    SubscriptionEligibilityService(Profile profile) {
        mNativeBridge = SubscriptionEligibilityServiceJni.get().createBridge(this, profile);
    }

    /** Cleans up the native bridge when the profile is destroyed. */
    void destroy() {
        if (mNativeBridge != 0) {
            SubscriptionEligibilityServiceJni.get().destroy(mNativeBridge);
            mNativeBridge = 0;
        }
    }

    /** Returns the current AI subscription tier. */
    public int getAiSubscriptionTier() {
        if (mNativeBridge == 0) return 0;
        return SubscriptionEligibilityServiceJni.get().getAiSubscriptionTier(mNativeBridge);
    }

    /** Adds an observer to be notified of tier changes. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** Removes an observer. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @CalledByNative
    private void onAiSubscriptionTierChanged() {
        for (Observer observer : mObservers) {
            observer.onAiSubscriptionTierChanged();
        }
    }

    @NativeMethods
    public interface Natives {
        long createBridge(SubscriptionEligibilityService caller, Profile profile);

        void destroy(long nativeSubscriptionEligibilityServiceBridge);

        int getAiSubscriptionTier(long nativeSubscriptionEligibilityServiceBridge);
    }
}
