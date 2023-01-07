// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.List;

/**
 * Provides storage for commerce subscription data.
 */
public class CommerceSubscriptionsStorage {
    private long mNativeCommerceSubscriptionDB;

    CommerceSubscriptionsStorage(Profile profile) {
        assert !profile.isOffTheRecord()
            : "CommerceSubscriptionsStorage is not supported for incognito profiles";
        CommerceSubscriptionsStorageJni.get().init(this, profile);
        assert mNativeCommerceSubscriptionDB != 0;
    }

    /**
     * Save one subscription to the database.
     * @param subscription The {@link CommerceSubscription} to store.
     */
    public void save(CommerceSubscription subscription) {
        assert mNativeCommerceSubscriptionDB != 0;
        saveWithCallback(subscription, null);
    }

    @MainThread
    @VisibleForTesting
    public void saveWithCallback(CommerceSubscription subscription, Runnable onComplete) {
        assert mNativeCommerceSubscriptionDB != 0;
        CommerceSubscriptionsStorageJni.get().save(mNativeCommerceSubscriptionDB,
                getKey(subscription), subscription.getType(), subscription.getTrackingId(),
                subscription.getManagementType(), subscription.getTrackingIdType(),
                subscription.getTimestamp(), onComplete);
    }

    /**
     * Load one subscription from the database.
     * @param key The key used to identify a subscription.
     * @param callback A callback with loaded result.
     */
    public void load(String key, Callback<CommerceSubscription> callback) {
        assert mNativeCommerceSubscriptionDB != 0;
        CommerceSubscriptionsStorageJni.get().load(mNativeCommerceSubscriptionDB, key, callback);
    }

    /**
     * Load all subscriptions whose keys have specific prefix.
     * @param prefix The prefix used to identify subscriptions.
     * @param callback A callback with loaded results.
     */
    public void loadWithPrefix(String prefix, Callback<List<CommerceSubscription>> callback) {
        assert mNativeCommerceSubscriptionDB != 0;
        CommerceSubscriptionsStorageJni.get().loadWithPrefix(
                mNativeCommerceSubscriptionDB, prefix, callback);
    }

    /**
     * Delete one subscription from the database.
     * @param subscription The {@link CommerceSubscription} to delete.
     */
    public void delete(CommerceSubscription subscription) {
        assert mNativeCommerceSubscriptionDB != 0;
        CommerceSubscriptionsStorageJni.get().delete(
                mNativeCommerceSubscriptionDB, getKey(subscription), null);
    }

    @MainThread
    @VisibleForTesting
    public void deleteForTesting(CommerceSubscription subscription, Runnable onComplete) {
        assert mNativeCommerceSubscriptionDB != 0;
        CommerceSubscriptionsStorageJni.get().delete(
                mNativeCommerceSubscriptionDB, getKey(subscription), onComplete);
    }

    /**
     * Delete all subscriptions from the database.
     */
    public void deleteAll() {
        assert mNativeCommerceSubscriptionDB != 0;
        CommerceSubscriptionsStorageJni.get().deleteAll(mNativeCommerceSubscriptionDB, null);
    }

    @MainThread
    @VisibleForTesting
    public void deleteAllForTesting(Runnable onComplete) {
        assert mNativeCommerceSubscriptionDB != 0;
        CommerceSubscriptionsStorageJni.get().deleteAll(mNativeCommerceSubscriptionDB, onComplete);
    }

    /**
     * Destroy the database.
     */
    public void destroy() {
        assert mNativeCommerceSubscriptionDB != 0;
        CommerceSubscriptionsStorageJni.get().destroy(mNativeCommerceSubscriptionDB);
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        assert nativePtr != 0;
        assert mNativeCommerceSubscriptionDB == 0;
        mNativeCommerceSubscriptionDB = nativePtr;
    }

    @VisibleForTesting
    public void setNativeCommerceSubscriptionDBForTesting(long nativeCommerceSubscriptionDB) {
        mNativeCommerceSubscriptionDB = nativeCommerceSubscriptionDB;
    }

    /**
     * Generate the key for a {@link CommerceSubscription} used to store it in database.
     * @param subscription The {@link CommerceSubscription} whose key we want to generate.
     */
    public static String getKey(CommerceSubscription subscription) {
        return String.format("%s_%s_%s", subscription.getType(), subscription.getTrackingIdType(),
                subscription.getTrackingId());
    }

    @NativeMethods
    interface Natives {
        void init(CommerceSubscriptionsStorage caller, BrowserContextHandle handle);
        void destroy(long nativeCommerceSubscriptionDB);
        void save(long nativeCommerceSubscriptionDB, String key, String type, String trackingId,
                String managementType, String trackingIdType, long timestamp, Runnable onComplete);
        void load(long nativeCommerceSubscriptionDB, String key,
                Callback<CommerceSubscription> callback);
        void loadWithPrefix(long nativeCommerceSubscriptionDB, String key,
                Callback<List<CommerceSubscription>> callback);
        void delete(long nativeCommerceSubscriptionDB, String key, Runnable onComplete);
        void deleteAll(long nativeCommerceSubscriptionDB, Runnable onComplete);
    }
}
