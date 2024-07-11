// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.offlinepages.background.UpdateRequestResult;
import org.chromium.net.NetworkChangeNotifier;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link RequestCoordinatorBridge}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class RequestCoordinatorBridgeTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final int TIMEOUT_MS = 5000;

    private RequestCoordinatorBridge mRequestCoordinatorBridge;
    private Profile mProfile;

    private void initializeBridgeForProfile() throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mRequestCoordinatorBridge = RequestCoordinatorBridge.getForProfile(mProfile);
                    semaphore.release();
                });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Ensure we start in an offline state.
                    NetworkChangeNotifier.forceConnectivityState(false);
                    if (!NetworkChangeNotifier.isInitialized()) {
                        NetworkChangeNotifier.init();
                    }
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = ProfileManager.getLastUsedRegularProfile();
                });

        initializeBridgeForProfile();

        List<Long> requestsToRemove = new ArrayList<>();
        for (SavePageRequest savePageRequest : OfflineTestUtil.getRequestsInQueue()) {
            requestsToRemove.add(Long.valueOf(savePageRequest.getRequestId()));
        }
        removeRequestsFromQueue(requestsToRemove);
    }

    @Test
    @MediumTest
    public void testGetRequestsInQueue() throws Exception {
        String url = "https://www.google.com/";
        String namespace = "custom_tabs";
        savePageLater(url, namespace);
        SavePageRequest[] requests = OfflineTestUtil.getRequestsInQueue();
        Assert.assertEquals(1, requests.length);
        Assert.assertEquals(namespace, requests[0].getClientId().getNamespace());
        Assert.assertEquals(url, requests[0].getUrl());

        String url2 = "https://mail.google.com/";
        String namespace2 = "last_n";
        savePageLater(url2, namespace2);
        requests = OfflineTestUtil.getRequestsInQueue();
        Assert.assertEquals(2, requests.length);

        HashSet<String> expectedUrls = new HashSet<>();
        expectedUrls.add(url);
        expectedUrls.add(url2);

        HashSet<String> expectedNamespaces = new HashSet<>();
        expectedNamespaces.add(namespace);
        expectedNamespaces.add(namespace2);

        for (SavePageRequest request : requests) {
            Assert.assertTrue(expectedNamespaces.contains(request.getClientId().getNamespace()));
            expectedNamespaces.remove(request.getClientId().getNamespace());
            Assert.assertTrue(expectedUrls.contains(request.getUrl()));
            expectedUrls.remove(request.getUrl());
        }
    }

    @Test
    @MediumTest
    public void testRequestCoordinatorBridgeDisabledInIncognitoTabbedActivity() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getPrimaryOTRProfile(/* createIfNeeded= */ true);
                });
        initializeBridgeForProfile();
        Assert.assertEquals(null, mRequestCoordinatorBridge);
    }

    @Test
    @MediumTest
    public void testRequestCoordinatorBridgeDisabledInIncognitoCCT() throws Exception {
        OTRProfileID otrProfileID = OTRProfileID.createUnique("CCT:Incognito");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileID, /* createIfNeeded= */ true);
                });
        initializeBridgeForProfile();
        Assert.assertEquals(null, mRequestCoordinatorBridge);
    }

    @Test
    @MediumTest
    public void testRemoveRequestsFromQueue() throws Exception {
        String url = "https://www.google.com/";
        String namespace = "custom_tabs";
        savePageLater(url, namespace);

        String url2 = "https://mail.google.com/";
        String namespace2 = "last_n";
        savePageLater(url2, namespace2);

        SavePageRequest[] requests = OfflineTestUtil.getRequestsInQueue();
        Assert.assertEquals(2, requests.length);

        List<Long> requestsToRemove = new ArrayList<>();
        requestsToRemove.add(Long.valueOf(requests[1].getRequestId()));

        List<RequestCoordinatorBridge.RequestRemovedResult> removed =
                removeRequestsFromQueue(requestsToRemove);
        Assert.assertEquals(requests[1].getRequestId(), removed.get(0).getRequestId());
        Assert.assertEquals(UpdateRequestResult.SUCCESS, removed.get(0).getUpdateRequestResult());
        SavePageRequest[] remaining = OfflineTestUtil.getRequestsInQueue();
        Assert.assertEquals(1, remaining.length);

        Assert.assertEquals(requests[0].getRequestId(), remaining[0].getRequestId());
        Assert.assertEquals(requests[0].getUrl(), remaining[0].getUrl());
    }

    private void savePageLater(final String url, final String namespace)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mRequestCoordinatorBridge.savePageLater(
                            url,
                            namespace,
                            /* userRequested= */ true,
                            new OfflinePageOrigin(),
                            new Callback<Integer>() {
                                @Override
                                public void onResult(Integer i) {
                                    Assert.assertEquals(
                                            "SavePageLater did not succeed",
                                            Integer.valueOf(0),
                                            i); // 0 is SUCCESS
                                    semaphore.release();
                                }
                            });
                });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private List<RequestCoordinatorBridge.RequestRemovedResult> removeRequestsFromQueue(
            final List<Long> requestsToRemove) throws InterruptedException {
        final AtomicReference<List<RequestCoordinatorBridge.RequestRemovedResult>> ref =
                new AtomicReference<>();
        final Semaphore semaphore = new Semaphore(0);
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mRequestCoordinatorBridge.removeRequestsFromQueue(
                            requestsToRemove,
                            new Callback<List<RequestCoordinatorBridge.RequestRemovedResult>>() {
                                @Override
                                public void onResult(
                                        List<RequestCoordinatorBridge.RequestRemovedResult>
                                                removedRequests) {
                                    ref.set(removedRequests);
                                    semaphore.release();
                                }
                            });
                });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return ref.get();
    }
}
