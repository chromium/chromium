// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Java side of the JNI bridge between GlicKeyedServiceImpl in Java and C++. All method calls are
 * delegated to the native C++ class.
 */
@JNINamespace("glic")
@NullMarked
public class GlicKeyedServiceImpl implements GlicKeyedService {
    private long mNativePtr;
    private final ObserverList<GlobalShowHideObserver> mObservers = new ObserverList<>();
    private final ObserverList<UserEnabledActuationOnWebObserver>
            mUserEnabledActuationOnWebObservers = new ObserverList<>();

    @CalledByNative
    private static GlicKeyedServiceImpl create(long nativePtr) {
        return new GlicKeyedServiceImpl(nativePtr);
    }

    private GlicKeyedServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @Override
    public void toggleUI(
            long browserWindowPtr, boolean preventClose, Profile profile, int invocationSource) {
        if (mNativePtr == 0) return;

        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.GLIC_ANDROID_USED);

        GlicKeyedServiceImplJni.get()
                .toggleUI(mNativePtr, browserWindowPtr, preventClose, profile, invocationSource);
    }

    @Override
    public boolean isPanelShowingForBrowser(long browserWindowPtr) {
        if (mNativePtr == 0) return false;
        return GlicKeyedServiceImplJni.get().isPanelShowingForBrowser(mNativePtr, browserWindowPtr);
    }

    @Override
    public boolean getUserEnabledActuationOnWeb() {
        if (mNativePtr == 0) return false;
        return GlicKeyedServiceImplJni.get().getUserEnabledActuationOnWeb(mNativePtr);
    }

    @Override
    public void setUserEnabledActuationOnWeb(boolean enabled) {
        if (mNativePtr == 0) return;
        GlicKeyedServiceImplJni.get().setUserEnabledActuationOnWeb(mNativePtr, enabled);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePtr = 0;
    }

    @Override
    public void addGlobalShowHideObserver(GlobalShowHideObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeGlobalShowHideObserver(GlobalShowHideObserver observer) {
        mObservers.removeObserver(observer);
    }

    @CalledByNative
    private void onGlobalShowHide(boolean isOpened) {
        for (GlobalShowHideObserver observer : mObservers) {
            observer.onGlobalShowHide(isOpened);
        }
    }

    @Override
    public void addUserEnabledActuationOnWebObserver(UserEnabledActuationOnWebObserver observer) {
        mUserEnabledActuationOnWebObservers.addObserver(observer);
    }

    @Override
    public void removeUserEnabledActuationOnWebObserver(
            UserEnabledActuationOnWebObserver observer) {
        mUserEnabledActuationOnWebObservers.removeObserver(observer);
    }

    @CalledByNative
    private void onUserEnabledActuationOnWebChanged(boolean enabled) {
        for (UserEnabledActuationOnWebObserver observer : mUserEnabledActuationOnWebObservers) {
            observer.onUserEnabledActuationOnWebChanged(enabled);
        }
    }

    @NativeMethods
    interface Natives {
        void toggleUI(
                long nativeGlicKeyedServiceAndroid,
                long browserWindowPtr,
                boolean preventClose,
                @JniType("Profile*") Profile profile,
                int source);

        boolean isPanelShowingForBrowser(long nativeGlicKeyedServiceAndroid, long browserWindowPtr);

        boolean getUserEnabledActuationOnWeb(long nativeGlicKeyedServiceAndroid);

        void setUserEnabledActuationOnWeb(long nativeGlicKeyedServiceAndroid, boolean enabled);
    }
}
