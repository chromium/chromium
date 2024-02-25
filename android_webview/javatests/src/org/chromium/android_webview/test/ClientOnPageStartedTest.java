// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.util.Pair;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedErrorHelper;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;

/** Tests for the ContentViewClient.onPageStarted() method. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class ClientOnPageStartedTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private TestWebServer mWebServer;
    private CallbackHelper mHangingRequestCallbackHelper;
    private Semaphore mHangingRequestSemaphore;
    private String mHangingUrl;
    private String mRedirectToHangingUrl;

    public ClientOnPageStartedTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setupTestServer() throws Exception {
        mWebServer = TestWebServer.start();
        mHangingRequestCallbackHelper = new CallbackHelper();
        mHangingRequestSemaphore = new Semaphore(0);
        Runnable hangingResponseRunnable =
                () -> {
                    mHangingRequestCallbackHelper.notifyCalled();
                    try {
                        mHangingRequestSemaphore.acquire();
                    } catch (Exception e) {
                        Assert.fail(e.getMessage());
                    }
                };

        mHangingUrl =
                mWebServer.setResponseWithRunnableAction(
                        "/hanging_page.html",
                        "<body>hanging page</body>",
                        null,
                        hangingResponseRunnable);
        mRedirectToHangingUrl = mWebServer.setRedirect("/redirect_to_hanging.html", mHangingUrl);
    }

    @After
    public void tearDownTestServer() {
        if (mHangingRequestSemaphore != null) {
            mHangingRequestSemaphore.release(9999);
        }
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
        mWebServer = null;
        mHangingRequestCallbackHelper = null;
        mHangingRequestSemaphore = null;
        mHangingUrl = null;
        mRedirectToHangingUrl = null;
    }

    @Before
    public void setUp() {
        setTestAwContentsClient(new TestAwContentsClient());
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
    }

    private void setTestAwContentsClient(TestAwContentsClient contentsClient) {
        mContentsClient = contentsClient;
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnPageStartedPassesCorrectUrl() throws Throwable {
        TestCallbackHelperContainer.OnPageStartedHelper onPageStartedHelper =
                mContentsClient.getOnPageStartedHelper();

        String html = "<html><body>Simple page.</body></html>";
        int currentCallCount = onPageStartedHelper.getCallCount();
        mActivityTestRule.loadDataAsync(mAwContents, html, "text/html", false);

        onPageStartedHelper.waitForCallback(currentCallCount);
        Assert.assertEquals("data:text/html," + html, onPageStartedHelper.getUrl());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnPageStartedCalledOnceOnError() throws Throwable {
        class LocalTestClient extends TestAwContentsClient {
            private boolean mIsOnReceivedErrorCalled;
            private boolean mIsOnPageStartedCalled;
            private boolean mAllowAboutBlank;

            @Override
            public void onReceivedError(AwWebResourceRequest request, AwWebResourceError error) {
                Assert.assertEquals(
                        "onReceivedError called twice for " + request.url,
                        false,
                        mIsOnReceivedErrorCalled);
                mIsOnReceivedErrorCalled = true;
                Assert.assertEquals(
                        "onPageStarted not called before onReceivedError for " + request.url,
                        true,
                        mIsOnPageStartedCalled);
                super.onReceivedError(request, error);
            }

            @Override
            public void onPageStarted(String url) {
                if (mAllowAboutBlank && ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL.equals(url)) {
                    super.onPageStarted(url);
                    return;
                }
                Assert.assertEquals(
                        "onPageStarted called twice for " + url, false, mIsOnPageStartedCalled);
                mIsOnPageStartedCalled = true;
                Assert.assertEquals(
                        "onReceivedError called before onPageStarted for " + url,
                        false,
                        mIsOnReceivedErrorCalled);
                super.onPageStarted(url);
            }

            void setAllowAboutBlank() {
                mAllowAboutBlank = true;
            }
        }

        LocalTestClient testContentsClient = new LocalTestClient();
        setTestAwContentsClient(testContentsClient);

        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        TestCallbackHelperContainer.OnPageStartedHelper onPageStartedHelper =
                mContentsClient.getOnPageStartedHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        String invalidUrl = "http://localhost:7/non_existent";
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, invalidUrl);

        Assert.assertEquals(invalidUrl, onReceivedErrorHelper.getRequest().url);
        Assert.assertEquals(invalidUrl, onPageStartedHelper.getUrl());

        // Rather than wait a fixed time to see that another onPageStarted callback isn't issued
        // we load a valid page. Since callbacks arrive sequentially, this will ensure that
        // any extra calls of onPageStarted / onReceivedError will arrive to our client.
        testContentsClient.setAllowAboutBlank();
        mActivityTestRule.loadUrlSync(
                mAwContents, onPageFinishedHelper, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotCalledForDownloadContentDisposition() throws Throwable {
        String data = "download data";
        String contentDisposition = "attachment;filename=\"download.txt\"";
        String mimeType = "text/plain";
        List<Pair<String, String>> downloadHeaders = new ArrayList<Pair<String, String>>();
        downloadHeaders.add(Pair.create("Content-Disposition", contentDisposition));
        downloadHeaders.add(Pair.create("Content-Type", mimeType));
        downloadHeaders.add(Pair.create("Content-Length", Integer.toString(data.length())));
        String downloadUrl = mWebServer.setResponse("/download.txt", data, downloadHeaders);

        doDownloadTest(downloadUrl);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotCalledForDownloadPdf() throws Throwable {
        String data = "";
        String mimeType = " application/pdf";
        List<Pair<String, String>> downloadHeaders = new ArrayList<Pair<String, String>>();
        downloadHeaders.add(Pair.create("Content-Type", mimeType));
        downloadHeaders.add(Pair.create("Content-Length", Integer.toString(data.length())));
        String downloadUrl = mWebServer.setResponse("/download.pdf", data, downloadHeaders);

        doDownloadTest(downloadUrl);
    }

    private void doDownloadTest(String downloadUrl) throws Throwable {
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");

        // Navigate to download from js.
        int downloadCount = mContentsClient.getOnDownloadStartHelper().getCallCount();
        int pageStartedCount = mContentsClient.getOnPageStartedHelper().getCallCount();
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        int shouldOverrideUrlLoadingCount =
                mContentsClient.getShouldOverrideUrlLoadingHelper().getCallCount();
        int onLoadResourceCount = mContentsClient.getOnLoadResourceHelper().getCallCount();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mAwContents.evaluateJavaScript(
                            "window.location.assign(\"" + downloadUrl + "\");", null);
                });

        // onPageStarted and onPageFinished should not be called.
        mContentsClient
                .getShouldOverrideUrlLoadingHelper()
                .waitForCallback(shouldOverrideUrlLoadingCount);
        mContentsClient.getOnDownloadStartHelper().waitForCallback(downloadCount);
        mContentsClient.getOnLoadResourceHelper().waitForCallback(onLoadResourceCount);
        Assert.assertEquals(
                downloadUrl,
                mContentsClient
                        .getShouldOverrideUrlLoadingHelper()
                        .getShouldOverrideUrlLoadingUrl());
        Assert.assertEquals(downloadUrl, mContentsClient.getOnDownloadStartHelper().getUrl());
        Assert.assertEquals(
                downloadUrl, mContentsClient.getOnLoadResourceHelper().getLastLoadedResource());
        Assert.assertEquals(
                pageStartedCount, mContentsClient.getOnPageStartedHelper().getCallCount());
        Assert.assertEquals(
                pageFinishedCount, mContentsClient.getOnPageFinishedHelper().getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRendererInitiatedHangingNavigation() throws Throwable {
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");

        // Load page that hangs (before semaphore is released).
        int pageStartedCount = mContentsClient.getOnPageStartedHelper().getCallCount();
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        int shouldOverrideUrlLoadingCount =
                mContentsClient.getShouldOverrideUrlLoadingHelper().getCallCount();
        int onLoadResourceCount = mContentsClient.getOnLoadResourceHelper().getCallCount();
        int hangingRequestCount = mHangingRequestCallbackHelper.getCallCount();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mAwContents.evaluateJavaScript(
                            "window.location.assign(\"" + mHangingUrl + "\");", null);
                });

        // onPageStarted and onPageFinished should not be called yet.
        mContentsClient
                .getShouldOverrideUrlLoadingHelper()
                .waitForCallback(shouldOverrideUrlLoadingCount);
        mContentsClient.getOnLoadResourceHelper().waitForCallback(onLoadResourceCount);
        mHangingRequestCallbackHelper.waitForCallback(hangingRequestCount);
        Assert.assertEquals(
                mHangingUrl,
                mContentsClient
                        .getShouldOverrideUrlLoadingHelper()
                        .getShouldOverrideUrlLoadingUrl());
        Assert.assertEquals(
                mHangingUrl, mContentsClient.getOnLoadResourceHelper().getLastLoadedResource());
        Assert.assertEquals(
                pageStartedCount, mContentsClient.getOnPageStartedHelper().getCallCount());
        Assert.assertEquals(
                pageFinishedCount, mContentsClient.getOnPageFinishedHelper().getCallCount());

        // Release request on server. Should get onPageStarted/Finished after.
        mHangingRequestSemaphore.release();
        mContentsClient.getOnPageStartedHelper().waitForCallback(pageStartedCount);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        Assert.assertEquals(mHangingUrl, mContentsClient.getOnPageStartedHelper().getUrl());
        Assert.assertEquals(mHangingUrl, mContentsClient.getOnPageFinishedHelper().getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testBrowserInitiatedHangingNavigation() throws Throwable {
        // Load page that hangs (before semaphore is released).
        int pageStartedCount = mContentsClient.getOnPageStartedHelper().getCallCount();
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        int onLoadResourceCount = mContentsClient.getOnLoadResourceHelper().getCallCount();
        int hangingRequestCount = mHangingRequestCallbackHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, mHangingUrl);

        // onPageStarted and onPageFinished should not be called yet.
        mContentsClient.getOnLoadResourceHelper().waitForCallback(onLoadResourceCount);
        mHangingRequestCallbackHelper.waitForCallback(hangingRequestCount);
        Assert.assertEquals(
                mHangingUrl, mContentsClient.getOnLoadResourceHelper().getLastLoadedResource());
        Assert.assertEquals(
                pageStartedCount, mContentsClient.getOnPageStartedHelper().getCallCount());
        Assert.assertEquals(
                pageFinishedCount, mContentsClient.getOnPageFinishedHelper().getCallCount());

        // Release request on server. Should get onPageStarted/Finished after.
        mHangingRequestSemaphore.release();
        mContentsClient.getOnPageStartedHelper().waitForCallback(pageStartedCount);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        Assert.assertEquals(mHangingUrl, mContentsClient.getOnPageStartedHelper().getUrl());
        Assert.assertEquals(mHangingUrl, mContentsClient.getOnPageFinishedHelper().getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRendererInitiatedRedirectHangingNavigation() throws Throwable {
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");

        // Load page that hangs (before semaphore is released).
        int pageStartedCount = mContentsClient.getOnPageStartedHelper().getCallCount();
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        int shouldOverrideUrlLoadingCount =
                mContentsClient.getShouldOverrideUrlLoadingHelper().getCallCount();
        int onLoadResourceCount = mContentsClient.getOnLoadResourceHelper().getCallCount();
        int hangingRequestCount = mHangingRequestCallbackHelper.getCallCount();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mAwContents.evaluateJavaScript(
                            "window.location.assign(\"" + mRedirectToHangingUrl + "\");", null);
                });

        // onPageStarted and onPageFinished should not be called yet.
        mContentsClient
                .getShouldOverrideUrlLoadingHelper()
                .waitForCallback(shouldOverrideUrlLoadingCount, 2);
        mContentsClient.getOnLoadResourceHelper().waitForCallback(onLoadResourceCount);
        mHangingRequestCallbackHelper.waitForCallback(hangingRequestCount);
        Assert.assertEquals(
                mHangingUrl,
                mContentsClient
                        .getShouldOverrideUrlLoadingHelper()
                        .getShouldOverrideUrlLoadingUrl());
        Assert.assertEquals(
                mRedirectToHangingUrl,
                mContentsClient.getOnLoadResourceHelper().getLastLoadedResource());
        Assert.assertEquals(
                pageStartedCount, mContentsClient.getOnPageStartedHelper().getCallCount());
        Assert.assertEquals(
                pageFinishedCount, mContentsClient.getOnPageFinishedHelper().getCallCount());

        // Release request on server. Should get onPageStarted/Finished after.
        mHangingRequestSemaphore.release();
        mContentsClient.getOnPageStartedHelper().waitForCallback(pageStartedCount);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        Assert.assertEquals(mHangingUrl, mContentsClient.getOnPageStartedHelper().getUrl());
        Assert.assertEquals(mHangingUrl, mContentsClient.getOnPageFinishedHelper().getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testBrowserInitiatedRedirectHangingNavigation() throws Throwable {
        // Load page that hangs (before semaphore is released).
        int pageStartedCount = mContentsClient.getOnPageStartedHelper().getCallCount();
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        int shouldOverrideUrlLoadingCount =
                mContentsClient.getShouldOverrideUrlLoadingHelper().getCallCount();
        int onLoadResourceCount = mContentsClient.getOnLoadResourceHelper().getCallCount();
        int hangingRequestCount = mHangingRequestCallbackHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, mRedirectToHangingUrl);

        // onPageStarted and onPageFinished should not be called yet.
        mContentsClient
                .getShouldOverrideUrlLoadingHelper()
                .waitForCallback(shouldOverrideUrlLoadingCount);
        mContentsClient.getOnLoadResourceHelper().waitForCallback(onLoadResourceCount);
        mHangingRequestCallbackHelper.waitForCallback(hangingRequestCount);
        Assert.assertEquals(
                mHangingUrl,
                mContentsClient
                        .getShouldOverrideUrlLoadingHelper()
                        .getShouldOverrideUrlLoadingUrl());
        Assert.assertEquals(
                mRedirectToHangingUrl,
                mContentsClient.getOnLoadResourceHelper().getLastLoadedResource());
        Assert.assertEquals(
                pageStartedCount, mContentsClient.getOnPageStartedHelper().getCallCount());
        Assert.assertEquals(
                pageFinishedCount, mContentsClient.getOnPageFinishedHelper().getCallCount());

        // Release request on server. Should get onPageStarted/Finished after.
        mHangingRequestSemaphore.release();
        mContentsClient.getOnPageStartedHelper().waitForCallback(pageStartedCount);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        Assert.assertEquals(mHangingUrl, mContentsClient.getOnPageStartedHelper().getUrl());
        Assert.assertEquals(mHangingUrl, mContentsClient.getOnPageFinishedHelper().getUrl());
    }
}
