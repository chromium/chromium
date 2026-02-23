// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

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

    @CalledByNative
    private static GlicKeyedServiceImpl create(long nativePtr) {
        return new GlicKeyedServiceImpl(nativePtr);
    }

    private GlicKeyedServiceImpl(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @Override
    public void toggleUI(long browserWindowPtr, Profile profile, int invocationSource) {
        if (mNativePtr == 0) return;

        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.GLIC_ANDROID_USED);

        GlicKeyedServiceImplJni.get()
                .toggleUI(mNativePtr, browserWindowPtr, profile, invocationSource);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        void toggleUI(
                long nativeGlicKeyedServiceAndroid,
                long browserWindowPtr,
                @JniType("Profile*") Profile profile,
                int source);
    }
}
