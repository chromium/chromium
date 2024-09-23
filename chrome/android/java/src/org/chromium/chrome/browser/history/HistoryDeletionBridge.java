// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** The JNI bridge for Android to receive notifications about history deletions. */
public class HistoryDeletionBridge implements Destroyable {
    /**
     * Allows derived class to listen to history deletions that pass through this bridge. The
     * HistoryDeletionInfo passed as a parameter is only valid for the duration of the method.
     */
    public interface Observer {
        void onURLsDeleted(HistoryDeletionInfo historyDeletionInfo);
    }

    private static ProfileKeyedMap<HistoryDeletionBridge> sProfileMap;

    /** Return the deletion bridge associated with the given {@link Profile}. */
    public static HistoryDeletionBridge getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        assert !profile.isOffTheRecord();
        if (sProfileMap == null) {
            sProfileMap = ProfileKeyedMap.createMapOfDestroyables();
        }
        return sProfileMap.getForProfile(profile, HistoryDeletionBridge::new);
    }

    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final long mNativeHistoryDeletionBridge;

    HistoryDeletionBridge(Profile profile) {
        mNativeHistoryDeletionBridge = HistoryDeletionBridgeJni.get().init(this, profile);
    }

    @Override
    public void destroy() {
        HistoryDeletionBridgeJni.get().destroy(mNativeHistoryDeletionBridge);
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @CalledByNative
    @VisibleForTesting
    void onURLsDeleted(HistoryDeletionInfo historyDeletionInfo) {
        for (Observer observer : mObservers) observer.onURLsDeleted(historyDeletionInfo);
    }

    @NativeMethods
    interface Natives {
        long init(HistoryDeletionBridge self, @JniType("Profile*") Profile profile);

        void destroy(long nativeHistoryDeletionBridge);
    }
}
