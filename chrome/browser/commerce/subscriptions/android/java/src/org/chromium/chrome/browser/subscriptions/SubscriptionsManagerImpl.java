// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import android.os.Build;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Queue;

/**
 * Implementation of {@link SubscriptionsManager} to manage price drop related subscriptions.
 * TODO(crbug.com/1186450): Pull subscription type specific code into respective handlers to
 * simplify this class.
 */
public class SubscriptionsManagerImpl implements SubscriptionsManager {
    @IntDef({Operation.SUBSCRIBE, Operation.UNSUBSCRIBE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Operation {
        int SUBSCRIBE = 0;
        int UNSUBSCRIBE = 1;
    }

    private final CommerceSubscriptionsStorage mStorage;
    private final CommerceSubscriptionsServiceProxy mServiceProxy;
    private static List<CommerceSubscription> sRemoteSubscriptionsForTesting;
    private boolean mCanHandleRequests;
    private Queue<DeferredSubscriptionOperation> mDeferredTasks;
    private final ObserverList<SubscriptionObserver> mObservers;
    private final PriceDropNotificationManager mPriceDropNotificationManager;

    private static class DeferredSubscriptionOperation {
        private final @Operation int mOperation;
        private final List<CommerceSubscription> mSubscriptions;
        private final Callback<Integer> mCallback;

        public DeferredSubscriptionOperation(@Operation int operation,
                List<CommerceSubscription> subscriptions, Callback<Integer> callback) {
            mOperation = operation;
            mSubscriptions = subscriptions;
            mCallback = callback;
        }

        public @Operation int getOperation() {
            return mOperation;
        }

        public List<CommerceSubscription> getSubscriptions() {
            return mSubscriptions;
        }

        public Callback<Integer> getCallback() {
            return mCallback;
        }
    }

    public SubscriptionsManagerImpl(
            Profile profile, PriceDropNotificationManager priceDropNotificationManager) {
        this(profile, new CommerceSubscriptionsStorage(profile),
                new CommerceSubscriptionsServiceProxy(profile), priceDropNotificationManager);
    }

    @VisibleForTesting
    SubscriptionsManagerImpl(Profile profile, CommerceSubscriptionsStorage storage,
            CommerceSubscriptionsServiceProxy proxy,
            PriceDropNotificationManager priceDropNotificationManager) {
        mStorage = storage;
        mServiceProxy = proxy;
        mPriceDropNotificationManager = priceDropNotificationManager;
        mDeferredTasks = new LinkedList<>();
        mCanHandleRequests = false;
        initTypes(this::onInitComplete);
        mObservers = new ObserverList<>();
    }

    @Override
    public void addObserver(SubscriptionObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(SubscriptionObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Creates a new subscription on the server-side and refreshes the local storage of
     * subscriptions.
     * @param subscription The {@link CommerceSubscription} to add.
     * @param callback indicates whether or not the operation was successful.
     */
    @Override
    public void subscribe(CommerceSubscription subscription, Callback<Integer> callback) {
        if (subscription == null || !isSubscriptionTypeSupported(subscription.getType())) {
            callback.onResult(SubscriptionsManager.StatusCode.INVALID_ARGUMENT);
            return;
        }

        subscribe(new ArrayList<CommerceSubscription>() {
            { add(subscription); };
        }, callback);
    }

    /**
     * Creates new subscriptions in batch if needed.
     * @param subscriptions The list of {@link CommerceSubscription} to add.
     * @param callback indicates whether or not the operation was successful.
     */
    @Override
    public void subscribe(List<CommerceSubscription> subscriptions, Callback<Integer> callback) {
        if (subscriptions.size() == 0) {
            callback.onResult(SubscriptionsManager.StatusCode.OK);
            return;
        }

        // Wrap the callback in one that allows us to trigger the observers.
        Callback<Integer> wrappedCallback = (status) -> {
            if (status == StatusCode.OK) {
                for (SubscriptionObserver o : mObservers) {
                    o.onSubscribe(subscriptions);
                }
            }
            callback.onResult(status);
        };

        String type = subscriptions.get(0).getType();
        if (!isSubscriptionTypeSupported(type)) {
            wrappedCallback.onResult(SubscriptionsManager.StatusCode.INVALID_ARGUMENT);
            return;
        }

        // Make sure the notification channel is initialized if there is a user-managed PRICE_TRACK
        // subscription. For chrome-managed subscriptions, channel will be initialized via message
        // card in tab switcher.
        if (CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK.equals(type)
                && CommerceSubscription.SubscriptionManagementType.USER_MANAGED.equals(
                        subscriptions.get(0).getManagementType())
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mPriceDropNotificationManager.createNotificationChannel();
        }

        if (!mCanHandleRequests) {
            mDeferredTasks.add(new DeferredSubscriptionOperation(
                    Operation.SUBSCRIBE, subscriptions, wrappedCallback));
            return;
        }

        getUniqueSubscriptions(subscriptions, (list) -> {
            if (list.size() == 0) {
                wrappedCallback.onResult(SubscriptionsManager.StatusCode.OK);
            } else {
                mServiceProxy.create(list,
                        (didSucceed)
                                -> handleUpdateSubscriptionsResponse(
                                        didSucceed, type, wrappedCallback));
            }
        });
    }

    /**
     * Destroys a subscription on the server-side and refreshes the local storage of subscriptions.
     * @param subscription The {@link CommerceSubscription} to destroy.
     * @param callback indicates whether or not the operation was successful.
     */
    @Override
    public void unsubscribe(CommerceSubscription subscription, Callback<Integer> callback) {
        String type = subscription.getType();
        if (subscription == null || !isSubscriptionTypeSupported(type)) {
            callback.onResult(SubscriptionsManager.StatusCode.INVALID_ARGUMENT);
            return;
        }
        unsubscribe(new ArrayList<CommerceSubscription>() {
            { add(subscription); };
        }, callback);
    }

    /**
     * Returns all subscriptions that match the provided type.
     * @param type The {@link CommerceSubscription.CommerceSubscriptionType} to query.
     * @param forceFetch Whether to fetch from server. If no, fetch from local storage.
     * @param callback returns the list of subscriptions.
     */
    @Override
    public void getSubscriptions(@CommerceSubscription.CommerceSubscriptionType String type,
            boolean forceFetch, Callback<List<CommerceSubscription>> callback) {
        if (sRemoteSubscriptionsForTesting != null) {
            callback.onResult(sRemoteSubscriptionsForTesting);
            return;
        }
        if (forceFetch) {
            mServiceProxy.get(type, callback);
        } else {
            mStorage.loadWithPrefix(String.valueOf(type),
                    localSubscriptions -> callback.onResult(localSubscriptions));
        }
    }

    /**
     * Checks if the given subscription matches any subscriptions in local storage.
     *
     * @param subscription The subscription to check.
     * @param callback The callback to receive the result.
     */
    @Override
    public void isSubscribed(CommerceSubscription subscription, Callback<Boolean> callback) {
        if (subscription == null) {
            callback.onResult(false);
            return;
        }

        // Searching by prefix instead of loading by key to handle cases of duplicates.
        String targetKey = CommerceSubscriptionsStorage.getKey(subscription);
        mStorage.loadWithPrefix(targetKey, localSubscriptions -> {
            // TODO: (crbug/1279519) CommerceSubscriptionsStorage should support full key matching
            // and we shouldn't need to perform this additional check.
            for (CommerceSubscription current : localSubscriptions) {
                if (targetKey.equals(CommerceSubscriptionsStorage.getKey(current))) {
                    callback.onResult(true);
                    return;
                }
            }
            callback.onResult(false);
        });
    }

    /**
     * Called when user account is cleared or updated.
     */
    void onIdentityChanged() {
        mStorage.deleteAll();
        // If the feature is still eligible to work, we should re-init and fetch the fresh data.
        if (PriceTrackingFeatures.isPriceDropNotificationEligible()) {
            initTypes((status) -> { assert status == SubscriptionsManager.StatusCode.OK; });
            queryAndUpdateWaaEnabled();
        }
    }

    /**
     * Query whether web and app activity is enabled on the server and update the local pref value.
     */
    public void queryAndUpdateWaaEnabled() {
        mServiceProxy.queryAndUpdateWaaEnabled();
    }

    @Override
    public void unsubscribe(List<CommerceSubscription> subscriptions, Callback<Integer> callback) {
        String type = subscriptions.get(0).getType();
        if (subscriptions == null || !isSubscriptionTypeSupported(type)) {
            callback.onResult(SubscriptionsManager.StatusCode.INVALID_ARGUMENT);
            return;
        }

        if (subscriptions.size() == 0) {
            callback.onResult(SubscriptionsManager.StatusCode.OK);
            return;
        }

        // Wrap the callback in one that allows us to trigger the observers.
        Callback<Integer> wrappedCallback = (status) -> {
            if (status == StatusCode.OK) {
                for (SubscriptionObserver o : mObservers) {
                    o.onUnsubscribe(subscriptions);
                }
            }
            callback.onResult(status);
        };

        if (!mCanHandleRequests) {
            mDeferredTasks.add(new DeferredSubscriptionOperation(
                    Operation.UNSUBSCRIBE, subscriptions, wrappedCallback));
            return;
        }

        Map<String, CommerceSubscription> subscriptionsMap = getSubscriptionsMap(subscriptions);
        mStorage.loadWithPrefix(String.valueOf(type), localSubscriptions -> {
            if (localSubscriptions.size() == 0) {
                wrappedCallback.onResult(SubscriptionsManager.StatusCode.OK);
                return;
            }

            List<CommerceSubscription> subscriptionsToDelete =
                    new ArrayList<CommerceSubscription>();

            for (CommerceSubscription current : localSubscriptions) {
                String key = CommerceSubscriptionsStorage.getKey(current);
                if (subscriptionsMap.containsKey(key)) {
                    subscriptionsToDelete.add(current);
                }
            }

            if (subscriptionsToDelete.size() == 0) {
                wrappedCallback.onResult(SubscriptionsManager.StatusCode.OK);
                return;
            }

            mServiceProxy.delete(subscriptionsToDelete,
                    (didSucceed)
                            -> handleUpdateSubscriptionsResponse(
                                    didSucceed, type, wrappedCallback));
        });
    }

    // Calls the backend for known types and updates the local cache.
    private void initTypes(Callback<Integer> callback) {
        mStorage.deleteAll();
        String type = CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK;
        getSubscriptions(type, true,
                remoteSubscriptions
                -> updateStorageWithSubscriptions(type, remoteSubscriptions, callback));
    }

    // Updates the local cache + the state of whether or not the object can start handling requests
    // based on the initial response form the server.
    private void onInitComplete(@SubscriptionsManager.StatusCode Integer result) {
        mCanHandleRequests = true;

        if (result == SubscriptionsManager.StatusCode.OK) {
            for (DeferredSubscriptionOperation item : mDeferredTasks) {
                if (Operation.SUBSCRIBE == item.getOperation()) {
                    subscribe(item.getSubscriptions(), item.getCallback());
                } else if (Operation.UNSUBSCRIBE == item.getOperation()) {
                    unsubscribe(item.getSubscriptions(), item.getCallback());
                }
            }
        } else {
            // Resolve all pending callbacks with an internal error and clear the queue.
            // TODO: add a retry in case of a network failure.
            for (DeferredSubscriptionOperation item : mDeferredTasks) {
                item.getCallback().onResult(SubscriptionsManager.StatusCode.INTERNAL_ERROR);
            }
        }

        mDeferredTasks.clear();
    }

    private boolean isSubscriptionTypeSupported(
            @CommerceSubscription.CommerceSubscriptionType String type) {
        return CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK.equals(type);
    }

    private void updateStorageWithSubscriptions(
            @CommerceSubscription.CommerceSubscriptionType String type,
            List<CommerceSubscription> remoteSubscriptions, Callback<Integer> callback) {
        mStorage.loadWithPrefix(String.valueOf(type), localSubscriptions -> {
            for (CommerceSubscription subscription : localSubscriptions) {
                if (!remoteSubscriptions.contains(subscription)) {
                    mStorage.delete(subscription);
                }
            }
            for (CommerceSubscription subscription : remoteSubscriptions) {
                if (!localSubscriptions.contains(subscription)) {
                    mStorage.save(subscription);
                }
            }

            callback.onResult(SubscriptionsManager.StatusCode.OK);
        });
    }

    private void handleUpdateSubscriptionsResponse(Boolean didSucceed,
            @CommerceSubscription.CommerceSubscriptionType String type,
            Callback<Integer> callback) {
        if (!didSucceed) {
            callback.onResult(SubscriptionsManager.StatusCode.NETWORK_ERROR);
            return;
        } else {
            getSubscriptions(type, true,
                    remoteSubscriptions
                    -> updateStorageWithSubscriptions(type, remoteSubscriptions, callback));
        }
    }

    // Creates a Key-Subscription map where key is generated using {@link
    // CommerceSubscriptionsStorage#getKey}.
    private Map<String, CommerceSubscription> getSubscriptionsMap(
            List<CommerceSubscription> subscriptions) {
        Map<String, CommerceSubscription> subscriptionsMap =
                new HashMap<String, CommerceSubscription>();
        for (CommerceSubscription current : subscriptions) {
            subscriptionsMap.put(CommerceSubscriptionsStorage.getKey(current), current);
        }

        return subscriptionsMap;
    }

    // Compares the provided subscriptions list against the local cache and only returns the ones
    // that are not in the local cache.
    private void getUniqueSubscriptions(List<CommerceSubscription> subscriptions,
            Callback<List<CommerceSubscription>> callback) {
        String type = subscriptions.get(0).getType();

        mStorage.loadWithPrefix(String.valueOf(type), localSubscriptions -> {
            if (localSubscriptions.size() == 0) {
                callback.onResult(subscriptions);
                return;
            }

            List<CommerceSubscription> result = new ArrayList<CommerceSubscription>();

            Map<String, CommerceSubscription> localSubscriptionsMap =
                    getSubscriptionsMap(localSubscriptions);

            for (CommerceSubscription subscription : subscriptions) {
                String key = CommerceSubscriptionsStorage.getKey(subscription);
                if (!localSubscriptionsMap.containsKey(key)) {
                    result.add(subscription);
                }
            }

            callback.onResult(result);
        });
    }

    @VisibleForTesting
    public void setRemoteSubscriptionsForTesting(List<CommerceSubscription> subscriptions) {
        sRemoteSubscriptionsForTesting = subscriptions;
    }

    @VisibleForTesting
    public void setCanHandlerequests(boolean value) {
        mCanHandleRequests = value;
    }
}
