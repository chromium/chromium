// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/** The JNI bridge for Android to receive notifications about history deletions. */
public class HistoryDeletionBridge {
    /**
     * Allows derived class to listen to history deletions that pass through this bridge. The
     * HistoryDeletionInfo passed as a parameter is only valid for the duration of the method.
     */
    public interface Observer { void onURLsDeleted(HistoryDeletionInfo historyDeletionInfo); }

    private static HistoryDeletionBridge sInstance;

    /**
     * @return Singleton instance for this class.
     */
    public static HistoryDeletionBridge getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new HistoryDeletionBridge();
        }

        return sInstance;
    }

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    HistoryDeletionBridge() {
        // This object is a singleton and therefore will be implicitly destroyed.
        HistoryDeletionBridgeJni.get().init(this);
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
        long init(HistoryDeletionBridge self);
    }
}
