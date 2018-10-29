// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import android.net.ConnectivityManager;
import android.net.Uri;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.download.internal.NetworkStatusListenerAndroid;
import org.chromium.components.gcm_driver.instance_id.FakeInstanceIDWithSubtype;
import org.chromium.components.offline_pages.core.prefetch.proto.StatusOuterClass;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.NetworkChangeNotifierAutoDetect;
import org.chromium.net.test.util.WebServer;

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for Prefetch.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PrefetchFlowTest implements WebServer.RequestHandler {
    private static final String TAG = "PrefetchFlowTest";
    private TestOfflinePageService mOPS = new TestOfflinePageService();
    private TestSuggestionsService mSuggestionsService = new TestSuggestionsService();
    private WebServer mServer;
    private Profile mProfile;
    private CallbackHelper mPageAddedHelper = new CallbackHelper();
    private List<OfflinePageItem> mAddedPages =
            Collections.synchronizedList(new ArrayList<OfflinePageItem>());

    // A fake NetworkChangeNotifierAutoDetect which always reports a connection.
    private static class FakeAutoDetect extends NetworkChangeNotifierAutoDetect {
        public FakeAutoDetect(Observer observer, RegistrationPolicy policy) {
            super(observer, policy);
        }
        @Override
        public NetworkState getCurrentNetworkState() {
            return new NetworkState(true, ConnectivityManager.TYPE_WIFI, 0, null, false);
        }
    }

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws Exception {
        // Start the server before Chrome starts, we use the server address in flags.
        mServer = new WebServer(0, false);
        mServer.setRequestHandler(this);
        // Inject FakeAutoDetect so that the download component will attempt to download a file even
        // when there is no connection.
        NetworkStatusListenerAndroid.setAutoDetectFactory(
                new NetworkStatusListenerAndroid.AutoDetectFactory() {
                    @Override
                    public NetworkChangeNotifierAutoDetect create(
                            NetworkChangeNotifierAutoDetect.Observer observer,
                            NetworkChangeNotifierAutoDetect.RegistrationPolicy policy) {
                        return new FakeAutoDetect(observer, policy);
                    }
                });

        // Configure flags:
        // - Enable OfflinePagesPrefetching
        // - Set DownloadService's start_up_delay_ms to 100 ms (5 seconds is default).
        // - Set offline_pages_backend.
        // - Set content_suggestions_backend.
        final String offlinePagesBackend = Uri.encode(mServer.getBaseUrl());
        final String suggestionsBackend = Uri.encode(mServer.getBaseUrl() + "suggestions/");
        CommandLine.getInstance().appendSwitchWithValue("enable-features",
                "OfflinePagesPrefetching<Trial,DownloadService<Trial,NTPArticleSuggestions<Trial");
        CommandLine.getInstance().appendSwitchWithValue("force-fieldtrials", "Trial/Group");
        CommandLine.getInstance().appendSwitchWithValue("force-fieldtrial-params",
                "Trial.Group:start_up_delay_ms/100/offline_pages_backend/" + offlinePagesBackend
                        + "/content_suggestions_backend/" + suggestionsBackend);

        // TestOfflinePageService will send GCM. Enable fake GCM.
        FakeInstanceIDWithSubtype.clearDataAndSetEnabled(true);

        // Start Chrome.
        mActivityTestRule.startMainActivityOnBlankPage();

        // Register Offline Page observer and enable limitless prefetching.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mProfile = mActivityTestRule.getActivity().getActivityTab().getProfile();
            OfflinePageBridge.getForProfile(mProfile).addObserver(
                    new OfflinePageBridge.OfflinePageModelObserver() {
                        @Override
                        public void offlinePageAdded(OfflinePageItem addedPage) {
                            mAddedPages.add(addedPage);
                            mPageAddedHelper.notifyCalled();
                        }
                    });
            PrefetchTestBridge.enableLimitlessPrefetching(true);
            PrefetchTestBridge.skipNTPSuggestionsAPIKeyCheck();
        });
    }

    @After
    public void tearDown() throws Exception {
        FakeInstanceIDWithSubtype.clearDataAndSetEnabled(false);
        mServer.shutdown();
    }

    @Override
    public void handleRequest(WebServer.HTTPRequest request, OutputStream stream) {
        try {
            if (request.getURI().startsWith("/suggestions")) {
                mSuggestionsService.handleRequest(request, stream);
            } else if (request.getURI().startsWith("/suggest_images")) {
                mSuggestionsService.writeImageResponse(stream);
            } else if (mOPS.handleRequest(request, stream)) {
                // handled.
            } else {
                Assert.fail("Unhandled request: " + request.toString());
            }
        } catch (IOException | org.json.JSONException e) {
            Assert.fail(e.getMessage() + " \n while handling request: " + request.toString());
        }
    }

    private void runAndWaitForBackgroundTask() throws Throwable {
        CallbackHelper finished = new CallbackHelper();
        PrefetchBackgroundTask task = new PrefetchBackgroundTask();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            TaskParameters.Builder builder =
                    TaskParameters.create(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
            PrefetchBackgroundTask.skipConditionCheckingForTesting();
            task.onStartTask(ContextUtils.getApplicationContext(), builder.build(),
                    (boolean needsReschedule) -> { finished.notifyCalled(); });
        });
        finished.waitForCallback(0);
    }

    /** Waits for |callbackHelper| to be notified. Runs the background task while waiting. */
    private void waitWithBackgroundTask(CallbackHelper callbackHelper, int currentCallCount,
            int numberOfCallsToWaitFor) throws Throwable {
        // It's necessary to run the background task multiple times because we don't always have a
        // hook to know when the system is ready for background processing.
        long startTime = System.nanoTime();
        while (true) {
            runAndWaitForBackgroundTask();
            try {
                callbackHelper.waitForCallback(
                        currentCallCount, numberOfCallsToWaitFor, 200, TimeUnit.MILLISECONDS);
                return;
            } catch (TimeoutException e) {
                if (System.nanoTime() - startTime > 5000000000L /*5 seconds*/) {
                    throw e;
                }
                continue;
            }
        }
    }

    /** Returns the URL prefix used to serve images from mServer. */
    private String imageUrlBase() {
        return mServer.getBaseUrl() + "suggest_images/";
    }

    /** Trigger conditions required to load NTP snippets. */
    private void forceLoadSnippets() throws Throwable {
        // NTP suggestions require a connection and an accepted EULA.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            NetworkChangeNotifier.forceConnectivityState(true);
            PrefServiceBridge.getInstance().setEulaAccepted();
        });

        // Loading the NTP triggers loading suggestions.
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
    }

    /** Serve a single suggestion to NTP snippets. That suggestion is successfully handled by
     * offline prefetch.
     */
    @Test
    @MediumTest
    @Feature({"OfflinePrefetch"})
    public void testPrefetchSinglePageSuccess() throws Throwable {
        // TODO(crbug.com/845310): Expand this test. There's some important flows missing and
        // systems missing:
        // - zine thumbnails

        final String url = "http://suggestion1.com/main";
        final String title = "Suggestion 1";
        // Serve the suggestion to NTP snippets, when snippets are requested.
        mSuggestionsService.addSuggestion(title, url, imageUrlBase() + "suggestion1");
        // Set up default (success) OPS behavior for the URL.
        TestOfflinePageService.PageBehavior behavior = new TestOfflinePageService.PageBehavior();
        mOPS.setPageBehavior(url, behavior);

        forceLoadSnippets();

        // Wait for the page to be added to the offline model.
        waitWithBackgroundTask(mPageAddedHelper, 0, 1);
        // Check that the thumbnail was fetched.
        // TODO(crbug.com/845310): Check that the thumbnail is shown in the download UI, or at least
        // that it is imported.
        Assert.assertEquals(1, mOPS.ReadCalled.getCallCount());

        Assert.assertEquals(1, mAddedPages.size());
        Assert.assertEquals(url, mAddedPages.get(0).getUrl());
        Assert.assertEquals(title, mAddedPages.get(0).getTitle());
        Assert.assertEquals(behavior.body.length, mAddedPages.get(0).getFileSize());
        Assert.assertNotEquals("", mAddedPages.get(0).getFilePath());
    }

    /** Request two pages. One is ready later, and one fails immediately. */
    @Test
    @MediumTest
    @Feature({"OfflinePrefetch"})
    public void testPrefetchPageReadyLater() throws Throwable {
        final String url1 = "http://suggestion1.com/main";
        mSuggestionsService.addSuggestion("Suggestion 1", url1, imageUrlBase() + "suggestion1");
        TestOfflinePageService.PageBehavior readyLater = new TestOfflinePageService.PageBehavior();
        readyLater.generateStatus = StatusOuterClass.Code.NOT_FOUND;
        mOPS.setPageBehavior(url1, readyLater);

        final String url2 = "http://suggestion2.com/main";
        mSuggestionsService.addSuggestion("Suggestion 2", url2, imageUrlBase() + "suggestion2");
        TestOfflinePageService.PageBehavior pageFail = new TestOfflinePageService.PageBehavior();
        pageFail.generateStatus = StatusOuterClass.Code.UNKNOWN;
        pageFail.getStatus = StatusOuterClass.Code.UNKNOWN;
        mOPS.setPageBehavior(url2, pageFail);

        forceLoadSnippets();
        waitWithBackgroundTask(mOPS.GeneratePageBundleCalled, 0, 1);

        // At this point, the bundle has been requested. Send the GCM message, and wait for the page
        // to be imported. If this assert fails, GeneratePageBundle may not have been called.
        Assert.assertEquals("operation-1", mOPS.sendPushMessage());
        runAndWaitForBackgroundTask();
        mPageAddedHelper.waitForCallback(0, 1);

        // Only url1 is added.
        Assert.assertEquals(1, mAddedPages.size());
        Assert.assertEquals(url1, mAddedPages.get(0).getUrl());
        Assert.assertEquals(readyLater.body.length, mAddedPages.get(0).getFileSize());
        Assert.assertNotEquals("", mAddedPages.get(0).getFilePath());
    }
}
