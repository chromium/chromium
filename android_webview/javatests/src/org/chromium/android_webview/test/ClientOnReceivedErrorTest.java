// Copyright 2012 The Chromium Authors
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
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.WebviewErrorCode;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedErrorHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;

import java.util.concurrent.TimeUnit;

/**
 * Tests for the ContentViewClient.onReceivedError() method. Tests for the
 * ContentViewClient.onReceivedError() method. Historically, this test suite focused on the basic
 * callback behavior from the 1st iteration of the callback. Now chromium only supports one version
 * of the callback, so the distinction between this and ClientOnReceivedError2Test.java is no longer
 * as significant.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class ClientOnReceivedErrorTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    // URLs which do not exist on the public internet (because they use the ".test" TLD).
    private static final String BAD_HTML_URL = "http://fake.domain.test/a.html";

    public ClientOnReceivedErrorTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

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
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();

        int onReceivedErrorCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, BAD_HTML_URL);

        // Verify that onReceivedError is called. The particular error code
        // that is returned depends on the configuration of the device (such as
        // existence of a proxy) so we don't test for it.
        onReceivedErrorHelper.waitForCallback(
                onReceivedErrorCount,
                /* numberOfCallsToWaitFor= */ 1,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        Assert.assertEquals(BAD_HTML_URL, onReceivedErrorHelper.getRequest().url);
        Assert.assertNotNull(onReceivedErrorHelper.getError().description);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOnReceivedErrorOnInvalidScheme() throws Throwable {
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();

        String url = "foo://some/resource";
        int onReceivedErrorCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCount);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_UNSUPPORTED_SCHEME,
                onReceivedErrorHelper.getError().errorCode);
        Assert.assertEquals(url, onReceivedErrorHelper.getRequest().url);
        Assert.assertNotNull(onReceivedErrorHelper.getError().description);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNoErrorOnFailedSubresourceLoad() throws Throwable {
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        int currentCallCount = onPageFinishedHelper.getCallCount();
        mActivityTestRule.loadDataAsync(
                mAwContents,
                "<html><iframe src=\"http//invalid.url.co/\" /></html>",
                "text/html",
                false);

        onPageFinishedHelper.waitForCallback(currentCallCount);
        Assert.assertEquals(0, onReceivedErrorHelper.getCallCount());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNonExistentAssetUrl() throws Throwable {
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        final String url = "file:///android_asset/does_not_exist.html";
        int onReceivedErrorCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCount);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_UNKNOWN, onReceivedErrorHelper.getError().errorCode);
        Assert.assertEquals(url, onReceivedErrorHelper.getRequest().url);
        Assert.assertNotNull(onReceivedErrorHelper.getError().description);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNonExistentResourceUrl() throws Throwable {
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        final String url = "file:///android_res/raw/does_not_exist.html";
        int onReceivedErrorCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCount);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_UNKNOWN, onReceivedErrorHelper.getError().errorCode);
        Assert.assertEquals(url, onReceivedErrorHelper.getRequest().url);
        Assert.assertNotNull(onReceivedErrorHelper.getError().description);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCacheMiss() throws Throwable {
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        final String url = "http://example.com/index.html";
        int onReceivedErrorCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule
                .getAwSettingsOnUiThread(mAwContents)
                .setCacheMode(WebSettings.LOAD_CACHE_ONLY);
        mActivityTestRule.loadUrlAsync(mAwContents, url);

        onReceivedErrorHelper.waitForCallback(onReceivedErrorCount);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_UNKNOWN, onReceivedErrorHelper.getError().errorCode);
        Assert.assertEquals(url, onReceivedErrorHelper.getRequest().url);
        Assert.assertFalse(onReceivedErrorHelper.getError().description.isEmpty());
    }
}
