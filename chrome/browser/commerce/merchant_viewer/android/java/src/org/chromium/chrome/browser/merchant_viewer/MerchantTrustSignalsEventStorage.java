// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.List;

/** Provides storage for merchant trust signals events. */
public class MerchantTrustSignalsEventStorage {
    private long mNativeMerchantSignalDB;
    private static boolean sSkipNativeAssertionsForTesting;

    MerchantTrustSignalsEventStorage(Profile profile) {
        assert !profile.isOffTheRecord()
                : "MerchantTrustSignalsEventStorage is not supported for incognito profiles";
        MerchantTrustSignalsEventStorageJni.get().init(this, profile);
        makeNativeAssertion();
    }

    /**
     * Save one event to the database.
     * @param event The {@link MerchantTrustSignalsEvent} to store.
     */
    public void save(MerchantTrustSignalsEvent event) {
        saveWithCallback(event, null);
    }

    @MainThread
    @VisibleForTesting
    public void saveWithCallback(MerchantTrustSignalsEvent event, Runnable onComplete) {
        makeNativeAssertion();
        MerchantTrustSignalsEventStorageJni.get()
                .save(mNativeMerchantSignalDB, event.getKey(), event.getTimestamp(), onComplete);
    }

    /**
     * Load one event from the database.
     * @param key The key used to identify a event.
     * @param callback A callback with loaded result.
     */
    public void load(String key, Callback<MerchantTrustSignalsEvent> callback) {
        makeNativeAssertion();
        MerchantTrustSignalsEventStorageJni.get().load(mNativeMerchantSignalDB, key, callback);
    }

    /**
     * Load all events whose keys have specific prefix.
     * @param prefix The prefix used to identify events.
     * @param callback A callback with loaded results.
     */
    public void loadWithPrefix(String prefix, Callback<List<MerchantTrustSignalsEvent>> callback) {
        makeNativeAssertion();
        MerchantTrustSignalsEventStorageJni.get()
                .loadWithPrefix(mNativeMerchantSignalDB, prefix, callback);
    }

    /**
     * Delete one event from the database.
     * @param event The {@link MerchantTrustSignalsEvent} to delete.
     */
    public void delete(MerchantTrustSignalsEvent event) {
        makeNativeAssertion();
        MerchantTrustSignalsEventStorageJni.get()
                .delete(mNativeMerchantSignalDB, event.getKey(), null);
    }

    @MainThread
    public void deleteForTesting(MerchantTrustSignalsEvent event, Runnable onComplete) {
        makeNativeAssertion();
        MerchantTrustSignalsEventStorageJni.get()
                .delete(mNativeMerchantSignalDB, event.getKey(), onComplete);
    }

    /** Delete all events from the database. */
    public void deleteAll() {
        makeNativeAssertion();
        MerchantTrustSignalsEventStorageJni.get().deleteAll(mNativeMerchantSignalDB, null);
    }

    @MainThread
    public void deleteAllForTesting(Runnable onComplete) {
        makeNativeAssertion();
        MerchantTrustSignalsEventStorageJni.get().deleteAll(mNativeMerchantSignalDB, onComplete);
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        if (!sSkipNativeAssertionsForTesting) {
            assert nativePtr != 0;
            assert mNativeMerchantSignalDB == 0;
        }
        mNativeMerchantSignalDB = nativePtr;
    }

    private void makeNativeAssertion() {
        if (!sSkipNativeAssertionsForTesting) {
            assert mNativeMerchantSignalDB != 0;
        }
    }

    static void setSkipNativeAssertionsForTesting(boolean skipNativeAssertionsForTesting) {
        sSkipNativeAssertionsForTesting = skipNativeAssertionsForTesting;
        ResettersForTesting.register(() -> sSkipNativeAssertionsForTesting = false);
    }

    @NativeMethods
    interface Natives {
        void init(MerchantTrustSignalsEventStorage caller, BrowserContextHandle handle);

        void save(long nativeMerchantSignalDB, String key, long timestamp, Runnable onComplete);

        void load(
                long nativeMerchantSignalDB,
                String key,
                Callback<MerchantTrustSignalsEvent> callback);

        void loadWithPrefix(
                long nativeMerchantSignalDB,
                String prefix,
                Callback<List<MerchantTrustSignalsEvent>> callback);

        void delete(long nativeMerchantSignalDB, String key, Runnable onComplete);

        void deleteAll(long nativeMerchantSignalDB, Runnable onComplete);
    }
}
