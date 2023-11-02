// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import androidx.annotation.Nullable;

import org.junit.Assert;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Test utility functions for OfflinePages. */
@JNINamespace("offline_pages")
public class OfflineTestUtil {
    // Forces request coordinator to process the requests in the queue.
    public static void startRequestCoordinatorProcessing() {
        TestThreadUtils.runOnUiThreadBlocking(() -> nativeStartRequestCoordinatorProcessing());
    }

    // Gets all the URLs in the request queue.
    public static SavePageRequest[] getRequestsInQueue() throws TimeoutException {
        final AtomicReference<SavePageRequest[]> result = new AtomicReference<>();
        final CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            nativeGetRequestsInQueue((SavePageRequest[] requests) -> {
                result.set(requests);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
        return result.get();
    }

    // Gets all available offline pages.
    public static List<OfflinePageItem> getAllPages() throws TimeoutException {
        final AtomicReference<List<OfflinePageItem>> result =
                new AtomicReference<List<OfflinePageItem>>();
        final CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            nativeGetAllPages(new ArrayList<OfflinePageItem>(), (List<OfflinePageItem> items) -> {
                result.set(items);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
        return result.get();
    }

    // Returns a string representation of the requests contained in the RequestCoordinator.
    // For logging out to debug test failures.
    public static String dumpRequestCoordinatorState() throws TimeoutException {
        final CallbackHelper callbackHelper = new CallbackHelper();
        final AtomicReference<String> result = new AtomicReference<String>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            nativeDumpRequestCoordinatorState((String dump) -> {
                result.set(dump);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
        return result.get();
    }

    // Returns the OfflinePageItem with the given clientId, or null if one doesn't exist.
    public static @Nullable OfflinePageItem getPageByClientId(ClientId clientId)
            throws TimeoutException {
        for (OfflinePageItem item : getAllPages()) {
            if (item.getClientId().equals(clientId)) {
                return item;
            }
        }
        return null;
    }

    // Returns all OfflineItems provided by the OfflineContentProvider.
    public static List<OfflineItem> getOfflineItems() throws TimeoutException {
        CallbackHelper finished = new CallbackHelper();
        final AtomicReference<ArrayList<OfflineItem>> result =
                new AtomicReference<ArrayList<OfflineItem>>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineContentAggregatorFactory.get().getAllItems(items -> {
                result.set(items);
                finished.notifyCalled();
            });
        });
        finished.waitForCallback(0);
        return result.get();
    }

    public static byte[] getRawThumbnail(long offlineId) throws TimeoutException {
        final AtomicReference<byte[]> result = new AtomicReference<>();
        final CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            nativeGetRawThumbnail(offlineId, (byte[] rawThumbnail) -> {
                result.set(rawThumbnail);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
        return result.get();
    }

    // Waits for the offline model to initialize and returns an OfflinePageBridge.
    public static OfflinePageBridge getOfflinePageBridge() throws TimeoutException {
        final CallbackHelper ready = new CallbackHelper();
        final AtomicReference<OfflinePageBridge> result = new AtomicReference<OfflinePageBridge>();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            OfflinePageBridge bridge =
                    OfflinePageBridge.getForProfile(Profile.getLastUsedRegularProfile());
            if (bridge == null || bridge.isOfflinePageModelLoaded()) {
                result.set(bridge);
                ready.notifyCalled();
                return;
            }
            bridge.addObserver(new OfflinePageModelObserver() {
                @Override
                public void offlinePageModelLoaded() {
                    result.set(bridge);
                    ready.notifyCalled();
                    bridge.removeObserver(this);
                }
            });
        });
        ready.waitForCallback(0);
        Assert.assertTrue(result.get() != null);
        return result.get();
    }

    // Intercepts future HTTP requests for |url| with an offline net error.
    public static void interceptWithOfflineError(String url) throws TimeoutException {
        final CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            nativeInterceptWithOfflineError(url, () -> callbackHelper.notifyCalled());
        });
        callbackHelper.waitForCallback(0);
    }

    // Clears all previous intercepts installed by interceptWithOfflineError.
    public static void clearIntercepts() {
        TestThreadUtils.runOnUiThreadBlocking(() -> nativeClearIntercepts());
    }

    // Waits for the connectivity state to change in the native network change notifier.
    public static void waitForConnectivityState(boolean connected) {
        AtomicBoolean done = new AtomicBoolean();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> nativeWaitForConnectivityState(connected, () -> done.set(true)));
        CriteriaHelper.pollInstrumentationThread(() -> done.get());
    }

    // Set the offline_pages.enabled_by_server pref for testing. If |enabled| is false,
    // also ensures that the server-enabled check is due.
    public static void setPrefetchingEnabledByServer(boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { nativeSetPrefetchingEnabledByServer(enabled); });
    }

    public static void setGCMTokenForTesting(String gcmToken) {
        TestThreadUtils.runOnUiThreadBlocking(() -> { nativeSetGCMTokenForTesting(gcmToken); });
    }

    private static native void nativeGetRequestsInQueue(Callback<SavePageRequest[]> callback);
    private static native void nativeGetAllPages(
            List<OfflinePageItem> offlinePages, final Callback<List<OfflinePageItem>> callback);
    private static native void nativeGetRawThumbnail(long offlineId, Callback<byte[]> callback);
    private static native void nativeStartRequestCoordinatorProcessing();
    private static native void nativeInterceptWithOfflineError(String url, Runnable readyRunnable);
    private static native void nativeClearIntercepts();
    private static native void nativeDumpRequestCoordinatorState(Callback<String> callback);
    private static native void nativeWaitForConnectivityState(boolean connected, Runnable callback);
    private static native void nativeSetPrefetchingEnabledByServer(boolean enabled);
    private static native void nativeSetGCMTokenForTesting(String gcmToken);
}
