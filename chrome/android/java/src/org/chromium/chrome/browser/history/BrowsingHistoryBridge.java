// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

import java.util.ArrayList;
import java.util.List;

/** The JNI bridge for Android to fetch and manipulate browsing history. */
public class BrowsingHistoryBridge implements HistoryProvider {
    private BrowsingHistoryObserver mObserver;
    private long mNativeHistoryBridge;
    private boolean mRemovingItems;
    private boolean mHasPendingRemoveRequest;

    public BrowsingHistoryBridge(boolean isIncognito) {
        mNativeHistoryBridge =
                BrowsingHistoryBridgeJni.get().init(BrowsingHistoryBridge.this, isIncognito);
    }

    @Override
    public void setObserver(BrowsingHistoryObserver observer) {
        mObserver = observer;
    }

    @Override
    public void destroy() {
        if (mNativeHistoryBridge != 0) {
            BrowsingHistoryBridgeJni.get().destroy(
                    mNativeHistoryBridge, BrowsingHistoryBridge.this);
            mNativeHistoryBridge = 0;
        }
    }

    @Override
    public void queryHistory(String query) {
        BrowsingHistoryBridgeJni.get().queryHistory(mNativeHistoryBridge,
                BrowsingHistoryBridge.this, new ArrayList<HistoryItem>(), query);
    }

    @Override
    public void queryHistoryContinuation() {
        BrowsingHistoryBridgeJni.get().queryHistoryContinuation(
                mNativeHistoryBridge, BrowsingHistoryBridge.this, new ArrayList<HistoryItem>());
    }

    @Override
    public void markItemForRemoval(HistoryItem item) {
        BrowsingHistoryBridgeJni.get().markItemForRemoval(mNativeHistoryBridge,
                BrowsingHistoryBridge.this, item.getUrl(), item.getNativeTimestamps());
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
        BrowsingHistoryBridgeJni.get().removeItems(
                mNativeHistoryBridge, BrowsingHistoryBridge.this);
    }

    @CalledByNative
    public static void createHistoryItemAndAddToList(List<HistoryItem> items, String url,
            String domain, String title, long mostRecentJavaTimestamp, long[] nativeTimestamps,
            boolean blockedVisit) {
        items.add(new HistoryItem(
                url, domain, title, mostRecentJavaTimestamp, nativeTimestamps, blockedVisit));
    }

    @CalledByNative
    public void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches) {
        if (mObserver != null) mObserver.onQueryHistoryComplete(items, hasMorePotentialMatches);
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
        long init(BrowsingHistoryBridge caller, boolean isIncognito);
        void destroy(long nativeBrowsingHistoryBridge, BrowsingHistoryBridge caller);
        void queryHistory(long nativeBrowsingHistoryBridge, BrowsingHistoryBridge caller,
                List<HistoryItem> historyItems, String query);
        void queryHistoryContinuation(long nativeBrowsingHistoryBridge,
                BrowsingHistoryBridge caller, List<HistoryItem> historyItems);
        void markItemForRemoval(long nativeBrowsingHistoryBridge, BrowsingHistoryBridge caller,
                String url, long[] nativeTimestamps);
        void removeItems(long nativeBrowsingHistoryBridge, BrowsingHistoryBridge caller);
    }
}
