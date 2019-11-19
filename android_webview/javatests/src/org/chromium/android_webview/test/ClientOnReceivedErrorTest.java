// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.support.test.filters.MediumTest;
import android.webkit.WebSettings;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.ErrorCodeConversionHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;

import java.util.concurrent.TimeUnit;

/**
 * Tests for the ContentViewClient.onReceivedError() method.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class ClientOnReceivedErrorTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

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
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();

        String url = "http://id.be.really.surprised.if.this.address.existed.blah/";
        int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        // Verify that onReceivedError is called. The particular error code
        // that is returned depends on the configuration of the device (such as
        // existence of a proxy) so we don't test for it.
        onReceivedErrorHelper.waitForCallback(onReceivedErrorCallCount,
                                              1 /* numberOfCallsToWaitFor */,
                                              WAIT_TIMEOUT_MS,
                                              TimeUnit.MILLISECONDS);
        Assert.assertEquals(url, onReceivedErrorHelper.getFailingUrl());
        Assert.assertNotNull(onReceivedErrorHelper.getDescription());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnReceivedErrorOnInvalidScheme() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();

        String url = "foo://some/resource";
        int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCallCount);
        Assert.assertEquals(ErrorCodeConversionHelper.ERROR_UNSUPPORTED_SCHEME,
                onReceivedErrorHelper.getErrorCode());
        Assert.assertEquals(url, onReceivedErrorHelper.getFailingUrl());
        Assert.assertNotNull(onReceivedErrorHelper.getDescription());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNoErrorOnFailedSubresourceLoad() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        int currentCallCount = onPageFinishedHelper.getCallCount();
        mActivityTestRule.loadDataAsync(mAwContents,
                "<html><iframe src=\"http//invalid.url.co/\" /></html>", "text/html", false);

        onPageFinishedHelper.waitForCallback(currentCallCount);
        Assert.assertEquals(0, onReceivedErrorHelper.getCallCount());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNonExistentAssetUrl() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        final String url = "file:///android_asset/does_not_exist.html";
        int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCallCount);
        Assert.assertEquals(
                ErrorCodeConversionHelper.ERROR_UNKNOWN, onReceivedErrorHelper.getErrorCode());
        Assert.assertEquals(url, onReceivedErrorHelper.getFailingUrl());
        Assert.assertNotNull(onReceivedErrorHelper.getDescription());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNonExistentResourceUrl() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        final String url = "file:///android_res/raw/does_not_exist.html";
        int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCallCount);
        Assert.assertEquals(
                ErrorCodeConversionHelper.ERROR_UNKNOWN, onReceivedErrorHelper.getErrorCode());
        Assert.assertEquals(url, onReceivedErrorHelper.getFailingUrl());
        Assert.assertNotNull(onReceivedErrorHelper.getDescription());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCacheMiss() throws Throwable {
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        final String url = "http://example.com/index.html";
        int onReceivedErrorCallCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents)
                .setCacheMode(WebSettings.LOAD_CACHE_ONLY);
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCallCount);
        Assert.assertEquals(
                ErrorCodeConversionHelper.ERROR_UNKNOWN, onReceivedErrorHelper.getErrorCode());
        Assert.assertEquals(url, onReceivedErrorHelper.getFailingUrl());
        Assert.assertFalse(onReceivedErrorHelper.getDescription().isEmpty());
    }
}
