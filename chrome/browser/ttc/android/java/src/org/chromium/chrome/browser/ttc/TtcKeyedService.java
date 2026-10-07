// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

/**
 * Java-side representation of the C++ ttc::TtcKeyedService. Lets Android UI start and end a TTC
 * session for a profile, and observe the session's state as the native side reports it. Obtain an
 * instance via {@link TtcKeyedServiceFactory#getForProfile}.
 */
@JNINamespace("ttc")
@NullMarked
public class TtcKeyedService {
    /** Observer for session-level events. All callbacks run on the UI thread. */
    public interface Observer {
        /** The service's {@link ServiceState} changed, e.g. a session started or ended. */
        default void onServiceStateChanged(@ServiceState int state) {}

        /** The backend is connected and microphone capture is live. */
        default void onSessionInitialized() {}

        /** The user's microphone input level changed. {@code level} is in [0, 1]. */
        default void onAudioLevelChanged(float level) {}

        /**
         * The session hit an error. {@code errorCode} is a {@code ttc::ErrorCode} value; the
         * session is ended by the native side shortly after.
         */
        default void onError(int errorCode) {}
    }

    private long mNativePtr;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    @CalledByNative
    private static TtcKeyedService create(long nativePtr) {
        return new TtcKeyedService(nativePtr);
    }

    private TtcKeyedService(long nativePtr) {
        mNativePtr = nativePtr;
    }

    /** Returns whether TTC can be used with this profile. */
    public boolean isEnabled() {
        ThreadUtils.assertOnUiThread();
        if (mNativePtr == 0) return false;
        return TtcKeyedServiceJni.get().isEnabled(mNativePtr);
    }

    /** Returns whether a session is currently in progress. */
    public boolean isSessionActive() {
        ThreadUtils.assertOnUiThread();
        if (mNativePtr == 0) return false;
        return TtcKeyedServiceJni.get().isSessionActive(mNativePtr);
    }

    /**
     * Starts a session. No-op if the service is disabled or a session is already active. The caller
     * is responsible for making sure the RECORD_AUDIO permission has been granted.
     */
    public void startSession() {
        ThreadUtils.assertOnUiThread();
        if (mNativePtr == 0) return;
        TtcKeyedServiceJni.get().startSession(mNativePtr);
    }

    /** Ends the active session, if any. */
    public void endSession() {
        ThreadUtils.assertOnUiThread();
        if (mNativePtr == 0) return;
        TtcKeyedServiceJni.get().endSession(mNativePtr);
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @CalledByNative
    private void onServiceStateChanged(@ServiceState int state) {
        for (Observer observer : mObservers) {
            observer.onServiceStateChanged(state);
        }
    }

    @CalledByNative
    private void onSessionInitialized() {
        for (Observer observer : mObservers) {
            observer.onSessionInitialized();
        }
    }

    @CalledByNative
    private void onAudioLevelChanged(float level) {
        for (Observer observer : mObservers) {
            observer.onAudioLevelChanged(level);
        }
    }

    @CalledByNative
    private void onError(int errorCode) {
        for (Observer observer : mObservers) {
            observer.onError(errorCode);
        }
    }

    @NativeMethods
    interface Natives {
        boolean isEnabled(long nativeTtcKeyedServiceAndroid);

        boolean isSessionActive(long nativeTtcKeyedServiceAndroid);

        void startSession(long nativeTtcKeyedServiceAndroid);

        void endSession(long nativeTtcKeyedServiceAndroid);
    }
}
