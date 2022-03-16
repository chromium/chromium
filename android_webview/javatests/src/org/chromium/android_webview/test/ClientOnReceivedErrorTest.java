// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.webkit.WebSettings;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.WebviewErrorCode;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedError2Helper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;

import java.util.concurrent.TimeUnit;

/**
 * Tests for the ContentViewClient.onReceivedError2() method. Tests for the
 * ContentViewClient.onReceivedError2() method. Historically, this test suite focused on the basic
 * callback behavior from the 1st iteration of the callback. Now chromium only supports one version
 * of the callback, so the distinction between this and ClientOnReceivedError2Test.java is no longer
 * as significant.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class ClientOnReceivedErrorTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    // URLs which do not exist on the public internet (because they use the ".test" TLD).
    private static final String BAD_HTML_URL = "http://fake.domain.test/a.html";

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnReceivedErrorOnInvalidUrl() throws Throwable {
        OnReceivedError2Helper onReceivedError2Helper = mContentsClient.getOnReceivedError2Helper();

        int onReceivedError2Count = onReceivedError2Helper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, BAD_HTML_URL);

        // Verify that onReceivedError is called. The particular error code
        // that is returned depends on the configuration of the device (such as
        // existence of a proxy) so we don't test for it.
        onReceivedError2Helper.waitForCallback(onReceivedError2Count,
                /* numberOfCallsToWaitFor= */ 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(BAD_HTML_URL, onReceivedError2Helper.getRequest().url);
        Assert.assertNotNull(onReceivedError2Helper.getError().description);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnReceivedErrorOnInvalidScheme() throws Throwable {
        OnReceivedError2Helper onReceivedError2Helper = mContentsClient.getOnReceivedError2Helper();

        String url = "foo://some/resource";
        int onReceivedError2Count = onReceivedError2Helper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        onReceivedError2Helper.waitForCallback(onReceivedError2Count);
        Assert.assertEquals(WebviewErrorCode.ERROR_UNSUPPORTED_SCHEME,
                onReceivedError2Helper.getError().errorCode);
        Assert.assertEquals(url, onReceivedError2Helper.getRequest().url);
        Assert.assertNotNull(onReceivedError2Helper.getError().description);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNoErrorOnFailedSubresourceLoad() throws Throwable {
        OnReceivedError2Helper onReceivedError2Helper = mContentsClient.getOnReceivedError2Helper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        int currentCallCount = onPageFinishedHelper.getCallCount();
        mActivityTestRule.loadDataAsync(mAwContents,
                "<html><iframe src=\"http//invalid.url.co/\" /></html>", "text/html", false);

        onPageFinishedHelper.waitForCallback(currentCallCount);
        Assert.assertEquals(0, onReceivedError2Helper.getCallCount());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNonExistentAssetUrl() throws Throwable {
        OnReceivedError2Helper onReceivedError2Helper = mContentsClient.getOnReceivedError2Helper();
        final String url = "file:///android_asset/does_not_exist.html";
        int onReceivedError2Count = onReceivedError2Helper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        onReceivedError2Helper.waitForCallback(onReceivedError2Count);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_UNKNOWN, onReceivedError2Helper.getError().errorCode);
        Assert.assertEquals(url, onReceivedError2Helper.getRequest().url);
        Assert.assertNotNull(onReceivedError2Helper.getError().description);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNonExistentResourceUrl() throws Throwable {
        OnReceivedError2Helper onReceivedError2Helper = mContentsClient.getOnReceivedError2Helper();
        final String url = "file:///android_res/raw/does_not_exist.html";
        int onReceivedError2Count = onReceivedError2Helper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        onReceivedError2Helper.waitForCallback(onReceivedError2Count);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_UNKNOWN, onReceivedError2Helper.getError().errorCode);
        Assert.assertEquals(url, onReceivedError2Helper.getRequest().url);
        Assert.assertNotNull(onReceivedError2Helper.getError().description);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCacheMiss() throws Throwable {
        OnReceivedError2Helper onReceivedError2Helper = mContentsClient.getOnReceivedError2Helper();
        final String url = "http://example.com/index.html";
        int onReceivedError2Count = onReceivedError2Helper.getCallCount();
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents)
                .setCacheMode(WebSettings.LOAD_CACHE_ONLY);
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        onReceivedError2Helper.waitForCallback(onReceivedError2Count);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_UNKNOWN, onReceivedError2Helper.getError().errorCode);
        Assert.assertEquals(url, onReceivedError2Helper.getRequest().url);
        Assert.assertFalse(onReceivedError2Helper.getError().description.isEmpty());
    }
}
