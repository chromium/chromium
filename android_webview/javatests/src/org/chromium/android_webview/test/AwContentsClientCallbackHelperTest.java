// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.graphics.Picture;
import android.os.Handler;
import android.os.Looper;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsClientCallbackHelper;
import org.chromium.android_webview.test.TestAwContentsClient.OnDownloadStartHelper;
import org.chromium.android_webview.test.TestAwContentsClient.OnLoadResourceHelper;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedErrorHelper;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedLoginRequestHelper;
import org.chromium.android_webview.test.TestAwContentsClient.PictureListenerHelper;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;

import java.util.concurrent.Callable;

/** Test suite for AwContentsClientCallbackHelper. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@OnlyRunIn(EITHER_PROCESS) // These are unit tests. No need to repeat in both modes.
@Batch(Batch.PER_CLASS)
public class AwContentsClientCallbackHelperTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static class TestCancelCallbackPoller
            implements AwContentsClientCallbackHelper.CancelCallbackPoller {
        private boolean mCancelled;
        private final CallbackHelper mCallbackHelper = new CallbackHelper();

        public void setCancelled() {
            mCancelled = true;
        }

        public CallbackHelper getCallbackHelper() {
            return mCallbackHelper;
        }

        @Override
        public boolean shouldCancelAllCallbacks() {
            mCallbackHelper.notifyCalled();
            return mCancelled;
        }
    }

    static final int PICTURE_TIMEOUT = 5000;
    static final String TEST_URL = "www.example.com";
    static final String REALM = "www.example.com";
    static final String ACCOUNT = "account";
    static final String ARGS = "args";
    static final String USER_AGENT = "userAgent";
    static final String CONTENT_DISPOSITION = "contentDisposition";
    static final String MIME_TYPE = "mimeType";
    static final int CONTENT_LENGTH = 42;

    static final float NEW_SCALE = 1.0f;
    static final float OLD_SCALE = 2.0f;
    static final int ERROR_CODE = 2;
    static final String ERROR_MESSAGE = "A horrible thing has occurred!";

    private TestAwContentsClient mContentsClient;
    private AwContentsClientCallbackHelper mClientHelper;
    private TestCancelCallbackPoller mCancelCallbackPoller;
    private Looper mLooper;

    public AwContentsClientCallbackHelperTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mLooper = Looper.getMainLooper();
        mContentsClient = new TestAwContentsClient();
        mClientHelper = new AwContentsClientCallbackHelper(mLooper, mContentsClient);
        mCancelCallbackPoller = new TestCancelCallbackPoller();
        mClientHelper.setCancelCallbackPoller(mCancelCallbackPoller);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnLoadResource() throws Exception {
        OnLoadResourceHelper loadResourceHelper = mContentsClient.getOnLoadResourceHelper();

        int onLoadResourceCount = loadResourceHelper.getCallCount();
        mClientHelper.postOnLoadResource(TEST_URL);
        loadResourceHelper.waitForCallback(onLoadResourceCount);
        Assert.assertEquals(TEST_URL, loadResourceHelper.getLastLoadedResource());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnPageStarted() throws Exception {
        OnPageStartedHelper pageStartedHelper = mContentsClient.getOnPageStartedHelper();

        int onPageStartedCount = pageStartedHelper.getCallCount();
        mClientHelper.postOnPageStarted(TEST_URL);
        pageStartedHelper.waitForCallback(onPageStartedCount);
        Assert.assertEquals(TEST_URL, pageStartedHelper.getUrl());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnDownloadStart() throws Exception {
        OnDownloadStartHelper downloadStartHelper = mContentsClient.getOnDownloadStartHelper();

        int onDownloadStartCount = downloadStartHelper.getCallCount();
        mClientHelper.postOnDownloadStart(
                TEST_URL, USER_AGENT, CONTENT_DISPOSITION, MIME_TYPE, CONTENT_LENGTH);
        downloadStartHelper.waitForCallback(onDownloadStartCount);
        Assert.assertEquals(TEST_URL, downloadStartHelper.getUrl());
        Assert.assertEquals(USER_AGENT, downloadStartHelper.getUserAgent());
        Assert.assertEquals(CONTENT_DISPOSITION, downloadStartHelper.getContentDisposition());
        Assert.assertEquals(MIME_TYPE, downloadStartHelper.getMimeType());
        Assert.assertEquals(CONTENT_LENGTH, downloadStartHelper.getContentLength());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnNewPicture() throws Exception {
        final PictureListenerHelper pictureListenerHelper =
                mContentsClient.getPictureListenerHelper();

        final Picture thePicture = new Picture();

        final Callable<Picture> pictureProvider = () -> thePicture;

        // AwContentsClientCallbackHelper rate limits photo callbacks so two posts in close
        // succession should only result in one callback.
        final int onNewPictureCount = pictureListenerHelper.getCallCount();
        // To trip the rate limiting the second postNewPicture call needs to happen
        // before mLooper processes the first. To do this we run both posts as a single block
        // and we do it in the thread that is processes the callbacks (mLooper).
        Handler mainHandler = new Handler(mLooper);
        Runnable postPictures =
                () -> {
                    mClientHelper.postOnNewPicture(pictureProvider);
                    mClientHelper.postOnNewPicture(pictureProvider);
                };
        mainHandler.post(postPictures);

        // We want to check that one and only one callback is fired,
        // First we wait for the first call back to complete, this ensures that both posts have
        // finished.
        pictureListenerHelper.waitForCallback(onNewPictureCount);

        // Then we post a runnable on the callback handler thread. Since both posts have happened
        // and the first callback has happened a second callback (if it exists) must be
        // in the queue before this runnable.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {});

        // When that runnable has finished we assert that one and only on callback happened.
        Assert.assertEquals(thePicture, pictureListenerHelper.getPicture());
        Assert.assertEquals(onNewPictureCount + 1, pictureListenerHelper.getCallCount());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnReceivedLoginRequest() throws Exception {
        OnReceivedLoginRequestHelper receivedLoginRequestHelper =
                mContentsClient.getOnReceivedLoginRequestHelper();

        int onReceivedLoginRequestCount = receivedLoginRequestHelper.getCallCount();
        mClientHelper.postOnReceivedLoginRequest(REALM, ACCOUNT, ARGS);
        receivedLoginRequestHelper.waitForCallback(onReceivedLoginRequestCount);
        Assert.assertEquals(REALM, receivedLoginRequestHelper.getRealm());
        Assert.assertEquals(ACCOUNT, receivedLoginRequestHelper.getAccount());
        Assert.assertEquals(ARGS, receivedLoginRequestHelper.getArgs());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnReceivedError() throws Exception {
        OnReceivedErrorHelper receivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();

        int onReceivedErrorCount = receivedErrorHelper.getCallCount();
        AwContentsClient.AwWebResourceRequest request = new AwContentsClient.AwWebResourceRequest();
        request.url = TEST_URL;
        request.isOutermostMainFrame = true;
        AwContentsClient.AwWebResourceError error = new AwContentsClient.AwWebResourceError();
        error.errorCode = ERROR_CODE;
        error.description = ERROR_MESSAGE;
        mClientHelper.postOnReceivedError(request, error);
        receivedErrorHelper.waitForCallback(onReceivedErrorCount);
        Assert.assertEquals(ERROR_CODE, receivedErrorHelper.getError().errorCode);
        Assert.assertEquals(ERROR_MESSAGE, receivedErrorHelper.getError().description);
        Assert.assertEquals(TEST_URL, receivedErrorHelper.getRequest().url);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testOnScaleChangedScaled() throws Exception {
        TestAwContentsClient.OnScaleChangedHelper scaleChangedHelper =
                mContentsClient.getOnScaleChangedHelper();

        int onScaleChangeCount = scaleChangedHelper.getCallCount();
        mClientHelper.postOnScaleChangedScaled(OLD_SCALE, NEW_SCALE);
        scaleChangedHelper.waitForCallback(onScaleChangeCount);
        Assert.assertEquals(OLD_SCALE, scaleChangedHelper.getOldScale(), 0);
        Assert.assertEquals(NEW_SCALE, scaleChangedHelper.getNewScale(), 0);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testCancelCallbackPoller() throws Exception {
        mCancelCallbackPoller.setCancelled();
        CallbackHelper cancelCallbackPollerHelper = mCancelCallbackPoller.getCallbackHelper();
        OnPageStartedHelper pageStartedHelper = mContentsClient.getOnPageStartedHelper();

        int pollCount = pageStartedHelper.getCallCount();
        int onPageStartedCount = pageStartedHelper.getCallCount();
        // Post two callbacks.
        mClientHelper.postOnPageStarted(TEST_URL);
        mClientHelper.postOnPageStarted(TEST_URL);

        // Wait for at least one poll.
        cancelCallbackPollerHelper.waitForCallback(pollCount);

        // Flush main queue.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {});

        // Neither callback should actually happen.
        Assert.assertEquals(onPageStartedCount, pageStartedHelper.getCallCount());
    }
}
