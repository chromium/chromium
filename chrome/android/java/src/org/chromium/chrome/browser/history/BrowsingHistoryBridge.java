// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.history.HistoryProvider.ClientInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** The JNI bridge for Android to fetch and manipulate browsing history. */
@NullMarked
public class BrowsingHistoryBridge implements HistoryProvider {
    private @Nullable BrowsingHistoryObserver mObserver;
    private long mNativeHistoryBridge;
    private boolean mRemovingItems;
    private boolean mHasPendingRemoveRequest;

    public BrowsingHistoryBridge(Profile profile) {
        mNativeHistoryBridge = BrowsingHistoryBridgeJni.get().init(this, profile);
    }

    @Override
    public void setObserver(BrowsingHistoryObserver observer) {
        mObserver = observer;
    }

    @Override
    public void destroy() {
        if (mNativeHistoryBridge != 0) {
            BrowsingHistoryBridgeJni.get().destroy(mNativeHistoryBridge);
            mNativeHistoryBridge = 0;
        }
    }

    @Override
    public void queryHistory(String query, QueryOptions options) {
        BrowsingHistoryBridgeJni.get()
                .queryHistory(
                        mNativeHistoryBridge,
                        new ArrayList<>(),
                        query,
                        options.appId,
                        options.hostName,
                        options.clientId);
    }

    @Override
    public void queryHistoryContinuation() {
        BrowsingHistoryBridgeJni.get()
                .queryHistoryContinuation(mNativeHistoryBridge, new ArrayList<>());
    }

    @Override
    public void queryApps() {
        BrowsingHistoryBridgeJni.get().getAllAppIds(mNativeHistoryBridge);
        // Native will call onQueryAppsComplete asynchronously.
    }

    @Override
    public void queryClients() {
        List<ClientInfo> clients =
                BrowsingHistoryBridgeJni.get().getAllClients(mNativeHistoryBridge);
        if (mObserver != null) mObserver.onQueryClientsComplete(clients);
    }

    @CalledByNative
    public static ClientInfo createClientInfo(
            @JniType("std::vector<std::string>") List<String> clientIds,
            @JniType("std::string") String clientName) {
        return new ClientInfo(clientIds, clientName);
    }

    @Override
    public void getLastVisitToHostBeforeRecentNavigations(
            String hostName, Callback<Long> callback) {
        BrowsingHistoryBridgeJni.get()
                .getLastVisitToHostBeforeRecentNavigations(
                        mNativeHistoryBridge, hostName, callback);
    }

    @Override
    public void markItemForRemoval(HistoryItem item) {
        BrowsingHistoryBridgeJni.get()
                .markItemForRemoval(
                        mNativeHistoryBridge,
                        item.getUrl(),
                        item.getAppId(),
                        item.getNativeTimestamps());
    }

    @Override
    public void removeItems() {
        // Only one remove request may be in-flight at any given time. If items are currently being
        // removed, queue the new request and return early.
        if (mRemovingItems) {
            mHasPendingRemoveRequest = true;
            return;
        }
        mRemovingItems = true;
        mHasPendingRemoveRequest = false;

        RecordHistogram.recordEnumeratedHistogram(
                "Privacy.DeleteBrowsingData.Action",
                DeleteBrowsingDataAction.HISTORY_PAGE_ENTRIES,
                DeleteBrowsingDataAction.MAX_VALUE + 1);

        BrowsingHistoryBridgeJni.get().removeItems(mNativeHistoryBridge);
    }

    @CalledByNative
    public static void createHistoryItemAndAddToList(
            List<HistoryItem> items,
            @JniType("GURL") GURL url,
            @JniType("std::u16string") String domain,
            @JniType("std::u16string") String title,
            @JniType("std::optional<std::string>") @Nullable String appId,
            long mostRecentJavaTimestamp,
            @JniType("std::vector<GURL>") List<GURL> urls,
            @JniType("std::vector<std::vector<int64_t>>") List<long[]> nativeTimestampsList,
            boolean blockedVisit,
            boolean isActorVisit) {
        assert urls.size() == nativeTimestampsList.size();
        Map<GURL, long[]> allTimestamps = new HashMap<>();
        for (int i = 0; i < urls.size(); i++) {
            allTimestamps.put(urls.get(i), nativeTimestampsList.get(i));
        }
        items.add(
                new HistoryItem(
                        url,
                        domain,
                        title,
                        appId,
                        mostRecentJavaTimestamp,
                        allTimestamps,
                        blockedVisit,
                        isActorVisit));
    }

    @CalledByNative
    public void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches) {
        if (mObserver != null) mObserver.onQueryHistoryComplete(items, hasMorePotentialMatches);
    }

    @CalledByNative
    public void onQueryAppsComplete(@JniType("std::vector<std::string>") List<String> items) {
        if (mObserver != null) mObserver.onQueryAppsComplete(items);
    }

    @CalledByNative
    public void onRemoveComplete() {
        mRemovingItems = false;
        if (mHasPendingRemoveRequest) removeItems();
    }

    @CalledByNative
    public void onRemoveFailed() {
        mRemovingItems = false;
        if (mHasPendingRemoveRequest) removeItems();
        // TODO(twellington): handle remove failures.
    }

    @CalledByNative
    public void onHistoryDeleted() {
        if (mObserver != null) mObserver.onHistoryDeleted();
    }

    @CalledByNative
    public void hasOtherFormsOfBrowsingData(boolean hasOtherForms) {
        if (mObserver != null) {
            mObserver.hasOtherFormsOfBrowsingData(hasOtherForms);
        }
    }

    @NativeMethods
    interface Natives {
        long init(BrowsingHistoryBridge self, @JniType("Profile*") Profile profile);

        void destroy(long nativeBrowsingHistoryBridge);

        void queryHistory(
                long nativeBrowsingHistoryBridge,
                List<HistoryItem> historyItems,
                @JniType("std::u16string") String query,
                @JniType("std::optional<std::string>") @Nullable String appId,
                @JniType("std::optional<std::string>") @Nullable String hostnameSuffix,
                @JniType("std::optional<std::string>") @Nullable String clientId);

        void queryHistoryContinuation(
                long nativeBrowsingHistoryBridge, List<HistoryItem> historyItems);

        void getLastVisitToHostBeforeRecentNavigations(
                long nativeBrowsingHistoryBridge,
                @JniType("std::string") String hostName,
                Callback<Long> callback);

        void markItemForRemoval(
                long nativeBrowsingHistoryBridge,
                @JniType("GURL") GURL url,
                @JniType("std::optional<std::string>") @Nullable String appId,
                @JniType("std::vector<int64_t>") long[] nativeTimestamps);

        void removeItems(long nativeBrowsingHistoryBridge);

        void getAllAppIds(long nativeBrowsingHistoryBridge);

        @JniType("std::vector<const syncer::DeviceInfo*>")
        List<ClientInfo> getAllClients(long nativeBrowsingHistoryBridge);
    }
}
