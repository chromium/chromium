// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Bundle;

import androidx.annotation.Keep;
import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwNoVarySearchData;
import org.chromium.android_webview.AwPrefetchCallback;
import org.chromium.android_webview.AwPrefetchManager;
import org.chromium.android_webview.AwPrefetchParameters;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.ServerCertificate;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Executor;

/**
 * This test should cover all WebView's expectations for Prefetch. Changing any of these tests
 * should be reflected in our API docs as they map to our API usage expectations.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests that need browser start are incompatible with @Batch")
public class AwPrefetchTest extends AwParameterizedTest {

    // Current tests doesn't require a complex webpage to test. Later on we may need to add specific
    // page with different resources in it.
    private static final String BASIC_PREFETCH_URL = "/android_webview/test/data/hello_world.html";

    private String mPrefetchUrl;

    public AwPrefetchTest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Rule public AwActivityTestRule mActivityTestRule;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startBrowserProcess();

        AwEmbeddedTestServer mTestServer =
                AwEmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_TEST_NAMES);

        mPrefetchUrl = mTestServer.getURLWithHostName("a.test", BASIC_PREFETCH_URL);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchRequestResponseSuccess() throws Throwable {
        // Prepare PrefetchParameters
        Map<String, String> additionalHeaders = new HashMap<>();
        additionalHeaders.put("foo", "bar");
        additionalHeaders.put("lorem", "ipsum");
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(additionalHeaders, expectedNoVarySearch, true);

        // Do the prefetch request.
        TestAwPrefetchCallback callback = startPrefetchingAndWait(mPrefetchUrl, prefetchParameters);

        // wait then do the checks
        callback.mOnStatusUpdatedHelper.waitForNext();
        Assert.assertEquals(1, callback.getOnStatusUpdatedHelper().getCallCount());
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback.getOnErrorHelper().mError);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchRequestHTTPSOnlySupported() throws Throwable {
        // Prepare PrefetchParameters
        Map<String, String> additionalHeaders = new HashMap<>();
        additionalHeaders.put("foo", "bar");
        additionalHeaders.put("lorem", "ipsum");
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(additionalHeaders, expectedNoVarySearch, true);

        // Do the prefetch request.
        TestAwPrefetchCallback callback =
                startPrefetchingAndWait("http://www.example.com", prefetchParameters);

        // wait then do the checks
        callback.mOnErrorHelper.waitForNext();
        Assert.assertEquals(1, callback.getOnErrorHelper().getCallCount());
        Assert.assertEquals(
                IllegalArgumentException.class, callback.getOnErrorHelper().getError().getClass());
        Assert.assertEquals(
                "URL must have HTTPS scheme for prefetch.",
                callback.getOnErrorHelper().getError().getMessage());
        Assert.assertNull(callback.getOnStatusUpdatedHelper().mExtras);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @Features.DisableFeatures({ContentFeatureList.PREFETCH_BROWSER_INITIATED_TRIGGERS})
    public void testPrefetchRequestFlagCheck() throws Throwable {
        // Prepare PrefetchParameters
        Map<String, String> additionalHeaders = new HashMap<>();
        additionalHeaders.put("foo", "bar");
        additionalHeaders.put("lorem", "ipsum");
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(additionalHeaders, expectedNoVarySearch, true);

        // Do the prefetch request.
        TestAwPrefetchCallback callback =
                startPrefetchingAndWait("https://www.example.com", prefetchParameters);

        // wait then do the checks
        callback.mOnErrorHelper.waitForNext();
        Assert.assertEquals(1, callback.getOnErrorHelper().getCallCount());
        Assert.assertEquals(
                IllegalStateException.class, callback.getOnErrorHelper().getError().getClass());
        Assert.assertEquals(
                "WebView initiated prefetching feature is not enabled.",
                callback.getOnErrorHelper().getError().getMessage());
        Assert.assertNull(callback.getOnStatusUpdatedHelper().mExtras);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchRequestInvalidHeaders() throws Throwable {
        final String[] invalids = {"null\u0000", "cr\r", "nl\n"};
        for (String invalid : invalids) {
            // try each invalid string as a key and a value
            // Prepare PrefetchParameters
            Map<String, String> additionalHeaders = new HashMap<>();
            additionalHeaders.put(invalid, "bar");
            AwNoVarySearchData expectedNoVarySearch =
                    new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
            AwPrefetchParameters prefetchParameters =
                    new AwPrefetchParameters(additionalHeaders, expectedNoVarySearch, true);

            // Do the prefetch request.
            TestAwPrefetchCallback callback =
                    startPrefetchingAndWait("https://www.example.com", prefetchParameters);

            // wait then do the checks
            callback.mOnErrorHelper.waitForNext();
            Assert.assertEquals(1, callback.getOnErrorHelper().getCallCount());
            Assert.assertEquals(
                    IllegalArgumentException.class,
                    callback.getOnErrorHelper().getError().getClass());
            Assert.assertEquals(
                    "HTTP headers must not contain null, CR, or NL characters. Invalid header name"
                            + " '"
                            + invalid
                            + "'.",
                    callback.getOnErrorHelper().getError().getMessage());
            Assert.assertNull(callback.getOnStatusUpdatedHelper().mExtras);
        }
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchRequestDuplicate() throws Throwable {
        // Prepare PrefetchParameters
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(null, expectedNoVarySearch, true);

        // Do the prefetch request.
        TestAwPrefetchCallback callback = startPrefetchingAndWait(mPrefetchUrl, prefetchParameters);

        // wait then do the checks
        callback.mOnStatusUpdatedHelper.waitForNext();
        Assert.assertEquals(1, callback.getOnStatusUpdatedHelper().getCallCount());
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback.getOnErrorHelper().mError);

        // Do another prefetch request but add the ignored query parameters.
        String prefetchUrlWithQueryParams = mPrefetchUrl + "?ts=1000&uid=007";
        TestAwPrefetchCallback callback2 =
                startPrefetchingAndWait(prefetchUrlWithQueryParams, prefetchParameters);

        // wait then do the checks
        callback2.mOnStatusUpdatedHelper.waitForNext();
        Assert.assertEquals(1, callback2.getOnStatusUpdatedHelper().getCallCount());
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.PREFETCH_START_FAILED_DUPLICATE,
                callback2.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback2.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback2.getOnErrorHelper().mError);

        // Finally, do a third request with an unexpected query parameter.
        String prefetchUrlWithUnexpectedQueryParam = prefetchUrlWithQueryParams + "&q=help";
        TestAwPrefetchCallback callback3 =
                startPrefetchingAndWait(prefetchUrlWithUnexpectedQueryParam, prefetchParameters);

        // wait then do the checks
        callback3.mOnStatusUpdatedHelper.waitForNext();
        Assert.assertEquals(1, callback3.getOnStatusUpdatedHelper().getCallCount());
        Assert.assertNotEquals(
                AwPrefetchCallback.StatusCode.PREFETCH_START_FAILED_DUPLICATE,
                callback3.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback3.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback3.getOnErrorHelper().mError);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchCancellation() throws Throwable {
        // Prepare PrefetchParameters
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(null, expectedNoVarySearch, true);

        // Do the prefetch request.
        TestAwPrefetchCallback callback = startPrefetchingAndWait(mPrefetchUrl, prefetchParameters);

        // Wait for the prefetch success & key for cancellation.
        callback.mOnStatusUpdatedHelper.waitForNext();
        Assert.assertEquals(1, callback.getOnStatusUpdatedHelper().getCallCount());
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback.getOnErrorHelper().mError);

        AwPrefetchManager prefetchManager =
                mActivityTestRule.getAwBrowserContext().getPrefetchManager();
        Assert.assertNotEquals(
                prefetchManager.getNoPrefetchKeyForTesting(), callback.getPrefetchKey());

        // Test cancellation.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final int prefetchKey = callback.getPrefetchKey();
                    Assert.assertTrue(prefetchManager.getIsPrefetchInCacheForTesting(prefetchKey));
                    mActivityTestRule
                            .getAwBrowserContext()
                            .getPrefetchManager()
                            .cancelPrefetch(prefetchKey);

                    // The prefetch for this key should no longer be in the cache after
                    // cancellation.
                    Assert.assertFalse(prefetchManager.getIsPrefetchInCacheForTesting(prefetchKey));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSettingConfigsWithInValidValues() {
        AwPrefetchManager prefetchManager =
                mActivityTestRule.getAwBrowserContext().getPrefetchManager();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Updating with negative values shouldn't be applied
                    prefetchManager.updatePrefetchConfiguration(-1, -1);
                    Assert.assertTrue(prefetchManager.getTTlInSec() > 0);
                    Assert.assertTrue(prefetchManager.getMaxPrefetches() > 0);

                    // Updating with 0 shouldn't be applied as well.
                    prefetchManager.updatePrefetchConfiguration(0, 0);
                    Assert.assertTrue(prefetchManager.getTTlInSec() > 0);
                    Assert.assertTrue(prefetchManager.getMaxPrefetches() > 0);
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSettingConfigsWithValidValues() {
        AwPrefetchManager prefetchManager =
                mActivityTestRule.getAwBrowserContext().getPrefetchManager();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    prefetchManager.updatePrefetchConfiguration(60, 5);
                    Assert.assertEquals(60, prefetchManager.getTTlInSec());
                    Assert.assertEquals(5, prefetchManager.getMaxPrefetches());
                });
    }

    private TestAwPrefetchCallback startPrefetchingAndWait(
            String url, AwPrefetchParameters prefetchParameters) {
        TestAwPrefetchCallback callback = new TestAwPrefetchCallback();

        Executor callbackExecutor = Runnable::run;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int prefetchKey =
                            mActivityTestRule
                                    .getAwBrowserContext()
                                    .getPrefetchManager()
                                    .startPrefetchRequest(
                                            url, prefetchParameters, callback, callbackExecutor);
                    callback.setPrefetchKey(prefetchKey);
                });

        return callback;
    }

    /**
     * A class to map the TestDelegate for handling the callback checks, see {@link CallbackHelper}
     * javadocs for more details.
     */
    private static class TestAwPrefetchCallback implements AwPrefetchCallback {

        public static class OnStatusUpdatedHelper extends CallbackHelper {
            private int mStatusCode = -1;
            private @Nullable Bundle mExtras;

            public int getStatusCode() {
                assert getCallCount() > 0;
                return mStatusCode;
            }

            @Keep
            @Nullable
            public Bundle getExtras() {
                assert getCallCount() > 0;
                return mExtras;
            }

            public void notifyCalled(int statusCode, @Nullable Bundle extras) {
                mStatusCode = statusCode;
                mExtras = extras;
                super.notifyCalled();
            }
        }

        public static class OnErrorHelper extends CallbackHelper {
            private Throwable mError;

            public Throwable getError() {
                assert getCallCount() > 0;
                return mError;
            }

            public void notifyCalled(Throwable error) {
                mError = error;
                super.notifyCalled();
            }
        }

        private final OnStatusUpdatedHelper mOnStatusUpdatedHelper;
        private final OnErrorHelper mOnErrorHelper;
        private int mPrefetchKey = -1;

        public TestAwPrefetchCallback() {
            mOnStatusUpdatedHelper = new OnStatusUpdatedHelper();
            mOnErrorHelper = new OnErrorHelper();
        }

        public OnStatusUpdatedHelper getOnStatusUpdatedHelper() {
            return mOnStatusUpdatedHelper;
        }

        public OnErrorHelper getOnErrorHelper() {
            return mOnErrorHelper;
        }

        public void setPrefetchKey(int prefetchKey) {
            mPrefetchKey = prefetchKey;
        }

        public int getPrefetchKey() {
            return mPrefetchKey;
        }

        @Override
        public void onStatusUpdated(int statusCode, @Nullable Bundle extras) {
            Log.e("Sayed", "Status code is" + statusCode);
            mOnStatusUpdatedHelper.notifyCalled(statusCode, extras);
        }

        @Override
        public void onError(Throwable e) {
            mOnErrorHelper.notifyCalled(e);
        }
    }
}
