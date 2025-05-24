// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/** Java access to the C++ offline_pages::RequestCoordinator. */
@JNINamespace("offline_pages::android")
public class RequestCoordinatorBridge {
    private final Profile mProfile;

    /**
     * Retrieves the RequestCoordinatorBridge for the given profile, creating it the first time
     * getForProfile is called for a given profile.  Must be called on the UI thread.
     *
     * @param profile The profile associated with the OfflinePageBridge to get.
     */
    public static @Nullable RequestCoordinatorBridge getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();

        if (profile.isOffTheRecord()) return null;

        return new RequestCoordinatorBridge(profile);
    }

    /** Creates a request coordinator bridge for a given profile. */
    @VisibleForTesting
    RequestCoordinatorBridge(Profile profile) {
        mProfile = profile;
    }

    /** Gets all the URLs in the request queue. */
    void getRequestsInQueue(Callback<SavePageRequest[]> callback) {
        RequestCoordinatorBridgeJni.get().getRequestsInQueue(mProfile, callback);
    }

    static class RequestsRemovedCallback {
        private final Callback<List<RequestRemovedResult>> mCallback;

        public RequestsRemovedCallback(Callback<List<RequestRemovedResult>> callback) {
            mCallback = callback;
        }

        @CalledByNative("RequestsRemovedCallback")
        public void onResult(long[] resultIds, int[] resultCodes) {
            assert resultIds.length == resultCodes.length;

            List<RequestRemovedResult> results = new ArrayList<>();
            for (int i = 0; i < resultIds.length; i++) {
                results.add(new RequestRemovedResult(resultIds[i], resultCodes[i]));
            }

            mCallback.onResult(results);
        }
    }

    /** Contains a result for a remove page request. */
    public static class RequestRemovedResult {
        private final long mRequestId;
        private final int mUpdateRequestResult;

        public RequestRemovedResult(long requestId, int requestResult) {
            mRequestId = requestId;
            mUpdateRequestResult = requestResult;
        }

        /** Request ID as found in the SavePageRequest. */
        public long getRequestId() {
            return mRequestId;
        }

        /** {@see org.chromium.components.offlinepages.background.UpdateRequestResult} enum. */
        public int getUpdateRequestResult() {
            return mUpdateRequestResult;
        }
    }

    /**
     * Removes SavePageRequests from the request queue.
     *
     * The callback will be called with |null| in the case that the queue is unavailable.  This can
     * happen in incognito, for example.
     *
     * @param requestIdList The IDs of the requests to remove.
     * @param callback Called when the removal is done, with the SavePageRequest objects that were
     *     actually removed.
     */
    public void removeRequestsFromQueue(
            List<Long> requestIdList, Callback<List<RequestRemovedResult>> callback) {
        long[] requestIds = new long[requestIdList.size()];
        for (int i = 0; i < requestIdList.size(); i++) {
            requestIds[i] = requestIdList.get(i).longValue();
        }
        RequestCoordinatorBridgeJni.get()
                .removeRequestsFromQueue(
                        mProfile, requestIds, new RequestsRemovedCallback(callback));
    }

    /**
     * Save the given URL as an offline page when the network becomes available.
     *
     * The page is marked as not having been saved by the user.  Use the 3-argument form to specify
     * a user request.
     *
     * @param url The given URL to save for later.
     * @param clientId The client ID for the offline page to be saved later.
     */
    @VisibleForTesting
    public void savePageLater(String url, ClientId clientId) {
        savePageLater(url, clientId, true);
    }

    /**
     * Save the given URL as an offline page when the network becomes available. Origin is
     * assumed to be Chrome.
     *
     * @param url The given URL to save for later.
     * @param clientId The client ID for the offline page to be saved later.
     * @param userRequested Whether this request should be prioritized because the user explicitly
     *     requested it.
     */
    public void savePageLater(final String url, final ClientId clientId, boolean userRequested) {
        savePageLater(url, clientId, userRequested, new OfflinePageOrigin());
    }

    /**
     * Save the given URL as an offline page when the network becomes available with the given
     * origin.
     *
     * @param url The given URL to save for later
     * @param clientId The clientId for the offline page to be saved later.
     * @param userRequested Whether this request should be prioritized because the user explicitly
     *                      requested it.
     * @param origin The app that initiated the request.
     */
    public void savePageLater(
            final String url,
            final ClientId clientId,
            boolean userRequested,
            OfflinePageOrigin origin) {
        savePageLater(url, clientId, userRequested, origin, null);
    }

    /**
     * Save the given URL as an offline page when the network becomes available with the given
     * origin. Callback with status when done.
     *
     * @param url The given URL to save for later.
     * @param clientId the clientId for the offline page to be saved later.
     * @param userRequested Whether this request should be prioritized because the user explicitly
     *                      requested it.
     * @param origin The app that initiated the request.
     * @param callback Callback for whether the URL is successfully added to queue. Non-zero number
     *                 represents a failure reason (See offline_pages::AddRequestResult enum). 0 is
     * success.
     */
    public void savePageLater(
            final String url,
            final ClientId clientId,
            boolean userRequested,
            OfflinePageOrigin origin,
            Callback<Integer> callback) {
        Callback<Integer> wrapper =
                new Callback<Integer>() {
                    @Override
                    public void onResult(Integer i) {
                        if (callback != null) {
                            callback.onResult(i);
                        }
                    }
                };
        RequestCoordinatorBridgeJni.get()
                .savePageLater(
                        mProfile,
                        wrapper,
                        url,
                        clientId.getNamespace(),
                        clientId.getId(),
                        origin.encodeAsJsonString(),
                        userRequested);
    }

    /**
     * Save the given URL as an offline page when the network becomes available with a randomly
     * generated clientId in the given namespace. Origin is defaulted to Chrome.
     *
     * @param url The given URL to save for later.
     * @param namespace The namespace for the offline page to be saved later.
     * @param userRequested Whether this request should be prioritized because the user explicitly
     *                      requested it.
     */
    public void savePageLater(final String url, final String namespace, boolean userRequested) {
        savePageLater(url, namespace, userRequested, new OfflinePageOrigin());
    }

    /**
     * Save the given URL as an offline page when the network becomes available with a randomly
     * generated clientId in the given namespace and the given origin.
     *
     * @param url The given URL to save for later
     * @param namespace The namespace for the offline page to be saved later.
     * @param userRequested Whether this request should be prioritized because the user explicitly
     *                      requested it.
     * @param origin The app that initiated the request.
     */
    public void savePageLater(
            final String url,
            final String namespace,
            boolean userRequested,
            OfflinePageOrigin origin) {
        savePageLater(url, namespace, userRequested, origin, null);
    }

    /**
     * Save the given URL as an offline page when the network becomes available with a randomly
     * generated clientId in the given namespace and the given origin. Calls back with whether
     * the URL has been successfully added to queue.
     *
     * @param url The given URL to save for later
     * @param namespace The namespace for the offline page to be saved later.
     * @param userRequested Whether this request should be prioritized because the user explicitly
     *                      requested it.
     * @param origin The app that initiated the request.
     * @param callback Callback to call whether the URL is successfully added to the queue. Non-zero
     *                 number represents failure reason (see offline_pages::AddRequestResult enum).
     * 0 is success.
     */
    public void savePageLater(
            final String url,
            final String namespace,
            boolean userRequested,
            OfflinePageOrigin origin,
            Callback<Integer> callback) {
        ClientId clientId = ClientId.createGuidClientIdForNamespace(namespace);
        savePageLater(url, clientId, userRequested, origin, callback);
    }

    @NativeMethods
    public interface Natives {
        void getRequestsInQueue(
                @JniType("Profile*") Profile profile, Callback<SavePageRequest[]> callback);

        void removeRequestsFromQueue(
                @JniType("Profile*") Profile profile,
                long[] requestIds,
                RequestsRemovedCallback callback);

        void savePageLater(
                @JniType("Profile*") Profile profile,
                Callback<Integer> callback,
                @JniType("std::string") String url,
                @JniType("std::string") String clientNamespace,
                @JniType("std::string") String clientId,
                @JniType("std::string") String origin,
                boolean userRequested);
    }
}
