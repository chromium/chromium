// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;

import java.util.List;

/**
 * Provides storage for merchant trust signals events.
 */
public class MerchantTrustSignalsEventStorage {
    private long mNativeMerchantSignalDB;

    MerchantTrustSignalsEventStorage(Profile profile) {
        assert !profile.isOffTheRecord()
            : "MerchantTrustSignalsEventStorage is not supported for incognito profiles";
        MerchantTrustSignalsEventStorageJni.get().init(this, profile);
        assert mNativeMerchantSignalDB != 0;
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
        assert mNativeMerchantSignalDB != 0;
        MerchantTrustSignalsEventStorageJni.get().save(
                mNativeMerchantSignalDB, event.getKey(), event.getTimestamp(), onComplete);
    }

    /**
     * Load one event from the database.
     * @param key The key used to identify a event.
     * @param callback A callback with loaded result.
     */
    public void load(String key, Callback<MerchantTrustSignalsEvent> callback) {
        assert mNativeMerchantSignalDB != 0;
        MerchantTrustSignalsEventStorageJni.get().load(mNativeMerchantSignalDB, key, callback);
    }

    /**
     * Load all events whose keys have specific prefix.
     * @param prefix The prefix used to identify events.
     * @param callback A callback with loaded results.
     */
    public void loadWithPrefix(String prefix, Callback<List<MerchantTrustSignalsEvent>> callback) {
        assert mNativeMerchantSignalDB != 0;
        MerchantTrustSignalsEventStorageJni.get().loadWithPrefix(
                mNativeMerchantSignalDB, prefix, callback);
    }

    /**
     * Delete one event from the database.
     * @param event The {@link MerchantTrustSignalsEvent} to delete.
     */
    public void delete(MerchantTrustSignalsEvent event) {
        assert mNativeMerchantSignalDB != 0;
        MerchantTrustSignalsEventStorageJni.get().delete(
                mNativeMerchantSignalDB, event.getKey(), null);
    }

    @MainThread
    @VisibleForTesting
    public void deleteForTesting(MerchantTrustSignalsEvent event, Runnable onComplete) {
        assert mNativeMerchantSignalDB != 0;
        MerchantTrustSignalsEventStorageJni.get().delete(
                mNativeMerchantSignalDB, event.getKey(), onComplete);
    }

    /**
     * Delete all events from the database.
     */
    public void deleteAll() {
        assert mNativeMerchantSignalDB != 0;
        MerchantTrustSignalsEventStorageJni.get().deleteAll(mNativeMerchantSignalDB, null);
    }

    @MainThread
    @VisibleForTesting
    public void deleteAllForTesting(Runnable onComplete) {
        assert mNativeMerchantSignalDB != 0;
        MerchantTrustSignalsEventStorageJni.get().deleteAll(mNativeMerchantSignalDB, onComplete);
    }

    /**
     * Destroy the database.
     */
    public void destroy() {
        assert mNativeMerchantSignalDB != 0;
        MerchantTrustSignalsEventStorageJni.get().destroy(mNativeMerchantSignalDB);
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        assert nativePtr != 0;
        assert mNativeMerchantSignalDB == 0;
        mNativeMerchantSignalDB = nativePtr;
    }

    @NativeMethods
    interface Natives {
        void init(MerchantTrustSignalsEventStorage caller, BrowserContextHandle handle);
        void destroy(long nativeMerchantSignalDB);
        void save(long nativeMerchantSignalDB, String key, long timestamp, Runnable onComplete);
        void load(long nativeMerchantSignalDB, String key,
                Callback<MerchantTrustSignalsEvent> callback);
        void loadWithPrefix(long nativeMerchantSignalDB, String prefix,
                Callback<List<MerchantTrustSignalsEvent>> callback);
        void delete(long nativeMerchantSignalDB, String key, Runnable onComplete);
        void deleteAll(long nativeMerchantSignalDB, Runnable onComplete);
    }
}
