// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import android.util.Pair;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwHttpCacheManager;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/** Tests for HTTP Cache Quota API. */
// Multi-profile generally requires multi-process.
@OnlyRunIn(MULTI_PROCESS)
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests focus on manipulation of global profile state")
@CommandLineFlags.Add({"enable-features=WebViewHttpCacheQuotaApi:Minimum/5242880/Maximum/41943040"})
public class HttpCacheQuotaTest extends AwParameterizedTest {
    // Should match the feature parameters above.
    private static final int MINIMUM = 5 * 1024 * 1024;
    private static final int MAXIMUM = 40 * 1024 * 1024;
    private static final int MEDIUM = 25 * 1024 * 1024;
    private static final String PROFILE_NAME = "CacheQuotaTestProfile";

    @Rule public MultiProfileTestRule mRule;

    private TestWebServer mWebServer;
    private final TestAwContentsClient mContentsClient;

    public HttpCacheQuotaTest(AwSettingsMutation param) {
        mRule = new MultiProfileTestRule(param.getMutation());
        mContentsClient = mRule.getContentsClient();
    }

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() throws Exception {
        mWebServer.shutdown();
    }

    // TODO(crbug.com/501449506): Add persistence test across sessions.
    //   This is not trivial right now as we don't have the testing framework to support it.

    private void testDisabledForProfile(AwBrowserContext profile) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Should still report meaningful defaults.
                    AwHttpCacheManager httpCacheManager = profile.getHttpCacheManager();

                    long defaultValue = httpCacheManager.getDefaultQuotaBytes();
                    Assert.assertTrue(defaultValue > 0);
                    Assert.assertTrue(defaultValue < Long.MAX_VALUE);

                    Assert.assertEquals(true, httpCacheManager.isUsingDefaultQuota());
                    Assert.assertEquals(defaultValue, httpCacheManager.getQuotaBytes());

                    httpCacheManager.setQuotaBytes(0);

                    Assert.assertEquals(true, httpCacheManager.isUsingDefaultQuota());
                    Assert.assertEquals(defaultValue, httpCacheManager.getQuotaBytes());

                    httpCacheManager.setQuotaBytes(Long.MAX_VALUE);

                    Assert.assertEquals(true, httpCacheManager.isUsingDefaultQuota());
                    Assert.assertEquals(defaultValue, httpCacheManager.getQuotaBytes());

                    httpCacheManager.useDefaultQuota();

                    Assert.assertEquals(true, httpCacheManager.isUsingDefaultQuota());
                    Assert.assertEquals(defaultValue, httpCacheManager.getQuotaBytes());

                    // Validation should continue to check arguments.
                    Assert.assertThrows(
                            IllegalArgumentException.class,
                            () -> {
                                httpCacheManager.setQuotaBytes(-1);
                            });
                });
    }

    private void testEnabledForProfile(AwBrowserContext profile) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwHttpCacheManager httpCacheManager = profile.getHttpCacheManager();

                    long defaultValue = httpCacheManager.getDefaultQuotaBytes();
                    Assert.assertTrue(defaultValue > 0);
                    Assert.assertTrue(defaultValue < Long.MAX_VALUE);

                    Assert.assertEquals(true, httpCacheManager.isUsingDefaultQuota());
                    Assert.assertEquals(defaultValue, httpCacheManager.getQuotaBytes());

                    httpCacheManager.setQuotaBytes(0);

                    Assert.assertEquals(false, httpCacheManager.isUsingDefaultQuota());
                    Assert.assertEquals(MINIMUM, httpCacheManager.getQuotaBytes());

                    httpCacheManager.setQuotaBytes(Long.MAX_VALUE);

                    Assert.assertEquals(false, httpCacheManager.isUsingDefaultQuota());
                    Assert.assertEquals(MAXIMUM, httpCacheManager.getQuotaBytes());

                    httpCacheManager.useDefaultQuota();

                    Assert.assertEquals(true, httpCacheManager.isUsingDefaultQuota());
                    Assert.assertEquals(defaultValue, httpCacheManager.getQuotaBytes());

                    Assert.assertThrows(
                            IllegalArgumentException.class,
                            () -> {
                                httpCacheManager.setQuotaBytes(-1);
                            });
                });
    }

    @Test
    @MediumTest
    @CommandLineFlags.Remove({
        "enable-features=WebViewHttpCacheQuotaApi:Minimum/5242880/Maximum/41943040"
    })
    @CommandLineFlags.Add({"disable-features=WebViewHttpCacheQuotaApi"})
    @Feature({"AndroidWebView"})
    public void testFeatureDisabled() throws Throwable {
        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);
        testDisabledForProfile(profile);

        AwBrowserContext defaultProfile =
                mRule.getProfileSync(AwBrowserContext.getDefaultContextName(), true);
        testDisabledForProfile(defaultProfile);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Remove({
        "enable-features=WebViewHttpCacheQuotaApi:Minimum/5242880/Maximum/41943040"
    })
    @CommandLineFlags.Add({
        "enable-features=WebViewHttpCacheQuotaApi:Minimum/5242880/Maximum/41943040/AllowForDefaultProfile/true"
    })
    @Feature({"AndroidWebView"})
    public void testAllowForDefaultProfileParameterEnabled() throws Throwable {
        AwBrowserContext defaultProfile =
                mRule.getProfileSync(AwBrowserContext.getDefaultContextName(), true);
        testEnabledForProfile(defaultProfile);

        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);
        testEnabledForProfile(profile);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Remove({
        "enable-features=WebViewHttpCacheQuotaApi:Minimum/5242880/Maximum/41943040"
    })
    @CommandLineFlags.Add({
        "enable-features=WebViewHttpCacheQuotaApi:Minimum/5242880/Maximum/41943040/AllowForDefaultProfile/false"
    })
    @Feature({"AndroidWebView"})
    public void testAllowForDefaultProfileParameterDisabled() throws Throwable {
        AwBrowserContext defaultProfile =
                mRule.getProfileSync(AwBrowserContext.getDefaultContextName(), true);
        testDisabledForProfile(defaultProfile);

        // Should still work for other profiles:
        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);
        testEnabledForProfile(profile);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testDefaultIsDefault() throws Throwable {
        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwHttpCacheManager httpCacheManager = profile.getHttpCacheManager();
                    Assert.assertEquals(true, httpCacheManager.isUsingDefaultQuota());
                    Assert.assertEquals(
                            httpCacheManager.getDefaultQuotaBytes(),
                            httpCacheManager.getQuotaBytes());
                    Assert.assertTrue(httpCacheManager.getQuotaBytes() > 0);
                    Assert.assertTrue(httpCacheManager.getQuotaBytes() < Long.MAX_VALUE);
                });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSettersAndGettersBeforeCacheInit() throws Throwable {
        final long quota1 = 10 * 1024 * 1024;
        final long quota2 = 20 * 1024 * 1024;

        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwHttpCacheManager httpCacheManager = profile.getHttpCacheManager();

                    httpCacheManager.setQuotaBytes(quota1);
                    Assert.assertEquals(quota1, httpCacheManager.getQuotaBytes());
                    Assert.assertEquals(false, httpCacheManager.isUsingDefaultQuota());

                    httpCacheManager.useDefaultQuota();
                    Assert.assertEquals(
                            httpCacheManager.getDefaultQuotaBytes(),
                            httpCacheManager.getQuotaBytes());
                    Assert.assertEquals(true, httpCacheManager.isUsingDefaultQuota());

                    httpCacheManager.setQuotaBytes(quota2);
                    Assert.assertEquals(quota2, httpCacheManager.getQuotaBytes());
                    Assert.assertEquals(false, httpCacheManager.isUsingDefaultQuota());
                });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSettersAndGettersAfterCacheInit() throws Throwable {
        final long quota1 = 10 * 1024 * 1024;
        final long quota2 = 20 * 1024 * 1024;

        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);

        AwContents awContents = mRule.createAwContents(profile);
        String url = mWebServer.setResponse("/", "This forces cache initialization", null);
        mRule.loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), url);
        mRule.destroyAwContentsOnMainSync(awContents);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwHttpCacheManager httpCacheManager = profile.getHttpCacheManager();
                    httpCacheManager.setQuotaBytes(quota1);
                    Assert.assertEquals(quota1, httpCacheManager.getQuotaBytes());
                    Assert.assertEquals(false, httpCacheManager.isUsingDefaultQuota());

                    httpCacheManager.useDefaultQuota();
                    Assert.assertEquals(
                            httpCacheManager.getDefaultQuotaBytes(),
                            httpCacheManager.getQuotaBytes());
                    Assert.assertEquals(true, httpCacheManager.isUsingDefaultQuota());

                    httpCacheManager.setQuotaBytes(quota2);
                    Assert.assertEquals(quota2, httpCacheManager.getQuotaBytes());
                    Assert.assertEquals(false, httpCacheManager.isUsingDefaultQuota());
                });
    }

    // Returns true if the cache could store the given quantity of data. Note that the cache may
    // have overheads or early eviction watermarks, so do not use borderline values.
    // Call on instrumentation thread.
    private boolean checkCacheSize(AwBrowserContext profile, int mebibytes) throws Throwable {
        final int resourceSize = 1024 * 1024;

        final AwContents awContents = mRule.createAwContents(profile);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    awContents.clearCache(true);
                    awContents.getSettings().setJavaScriptEnabled(true);
                });

        List<Pair<String, String>> headers = new ArrayList<>();
        headers.add(new Pair<>("Cache-Control", "max-age=3600"));

        // Use random data to defend against any theoretical space optimizations (compression or
        // sparse files).
        Random random = new Random(42);
        for (int i = 0; i < mebibytes; i++) {
            String path = "/resource" + i;
            byte[] data = new byte[resourceSize];
            random.nextBytes(data);
            mWebServer.setResponse(path, data, headers);
        }

        final String html =
                String.format(
                        """
                        <html>
                        <body>
                        <script>
                        (async () => {
                            for (let i = 0; i < %d; i++) {
                                const response =
                                        await fetch("/resource" + i, {mode: "same-origin"});
                                // Avoid concurrency by awaiting on the entire content body.
                                await response.arrayBuffer();
                            }
                            document.title = "finished";
                        })();
                        </script>
                        </body>
                        </html>
                        """,
                        mebibytes);

        // Load all resources once.
        mRule.loadDataWithBaseUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                html,
                "text/html",
                false,
                mWebServer.getBaseUrl(),
                mWebServer.getBaseUrl());
        mRule.pollUiThread(() -> "finished".equals(awContents.getTitle()));

        for (int i = 0; i < mebibytes; i++) {
            String path = "/resource" + i;
            Assert.assertEquals(1, mWebServer.getRequestCount(path));
        }

        // Load all resources again, possibly hitting the cache.
        mRule.loadDataWithBaseUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                html,
                "text/html",
                false,
                mWebServer.getBaseUrl(),
                mWebServer.getBaseUrl());
        mRule.pollUiThread(() -> "finished".equals(awContents.getTitle()));

        mRule.destroyAwContentsOnMainSync(awContents);

        // If anything reached out to the server again, there was a cache miss.
        for (int i = 0; i < mebibytes; i++) {
            String path = "/resource" + i;
            Assert.assertTrue(mWebServer.getRequestCount(path) <= 2);
            if (mWebServer.getRequestCount(path) != 1) {
                return false;
            }
        }

        // Everything got served from cache.
        return true;
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSmallCacheSetBeforeCacheInit() throws Throwable {
        final long quota = 5 * 1024 * 1024;

        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    profile.getHttpCacheManager().setQuotaBytes(quota);
                });

        Assert.assertFalse(checkCacheSize(profile, 10));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSmallCacheSetAfterCacheInit() throws Throwable {
        final long quota = 5 * 1024 * 1024;

        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);

        AwContents awContents = mRule.createAwContents(profile);
        String url = mWebServer.setResponse("/", "This forces cache initialization", null);
        mRule.loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), url);
        mRule.destroyAwContentsOnMainSync(awContents);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    profile.getHttpCacheManager().setQuotaBytes(quota);
                });

        Assert.assertFalse(checkCacheSize(profile, 10));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testLargeCacheSetBeforeCacheInit() throws Throwable {
        final long quota = 40 * 1024 * 1024;

        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    profile.getHttpCacheManager().setQuotaBytes(quota);
                });

        Assert.assertTrue(checkCacheSize(profile, 10));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testLargeCacheSetAfterCacheInit() throws Throwable {
        final long quota = 40 * 1024 * 1024;

        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);

        AwContents awContents = mRule.createAwContents(profile);
        String url = mWebServer.setResponse("/", "This forces cache initialization", null);
        mRule.loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), url);
        mRule.destroyAwContentsOnMainSync(awContents);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    profile.getHttpCacheManager().setQuotaBytes(quota);
                });

        Assert.assertTrue(checkCacheSize(profile, 10));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testProfilesCanHaveDifferentQuotas() throws Throwable {
        final long quota1 = 5 * 1024 * 1024;
        final long quota2 = 40 * 1024 * 1024;
        AwBrowserContext profile1 = mRule.getProfileSync("CacheQuotaTestProfile1", true);
        AwBrowserContext profile2 = mRule.getProfileSync("CacheQuotaTestProfile2", true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwHttpCacheManager httpCacheManager1 = profile1.getHttpCacheManager();
                    AwHttpCacheManager httpCacheManager2 = profile2.getHttpCacheManager();
                    Assert.assertTrue(httpCacheManager1.isUsingDefaultQuota());
                    Assert.assertTrue(httpCacheManager2.isUsingDefaultQuota());

                    httpCacheManager1.setQuotaBytes(quota1);

                    Assert.assertEquals(quota1, httpCacheManager1.getQuotaBytes());
                    Assert.assertFalse(httpCacheManager1.isUsingDefaultQuota());
                    Assert.assertTrue(httpCacheManager2.isUsingDefaultQuota());

                    httpCacheManager2.setQuotaBytes(quota2);

                    Assert.assertEquals(quota1, httpCacheManager1.getQuotaBytes());
                    Assert.assertEquals(quota2, httpCacheManager2.getQuotaBytes());
                    Assert.assertFalse(httpCacheManager1.isUsingDefaultQuota());
                    Assert.assertFalse(httpCacheManager2.isUsingDefaultQuota());
                });

        Assert.assertFalse(checkCacheSize(profile1, 10));
        Assert.assertTrue(checkCacheSize(profile2, 10));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwHttpCacheManager httpCacheManager1 = profile1.getHttpCacheManager();
                    AwHttpCacheManager httpCacheManager2 = profile2.getHttpCacheManager();
                    httpCacheManager1.useDefaultQuota();

                    Assert.assertEquals(
                            httpCacheManager1.getDefaultQuotaBytes(),
                            httpCacheManager1.getQuotaBytes());
                    Assert.assertEquals(quota2, httpCacheManager2.getQuotaBytes());
                    Assert.assertTrue(httpCacheManager1.isUsingDefaultQuota());
                    Assert.assertFalse(httpCacheManager2.isUsingDefaultQuota());
                });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testExtremeLegalValues() throws Throwable {
        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);

        // Call setQuotaBytes(MEDIUM) first to ensure that extreme values aren't simply ignored.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwHttpCacheManager httpCacheManager = profile.getHttpCacheManager();
                    httpCacheManager.setQuotaBytes(MEDIUM);
                    Assert.assertEquals(MEDIUM, httpCacheManager.getQuotaBytes());

                    httpCacheManager.setQuotaBytes(MEDIUM);
                    httpCacheManager.setQuotaBytes(0);
                    Assert.assertEquals(MINIMUM, httpCacheManager.getQuotaBytes());

                    httpCacheManager.setQuotaBytes(MEDIUM);
                    httpCacheManager.setQuotaBytes(1);
                    Assert.assertEquals(MINIMUM, httpCacheManager.getQuotaBytes());

                    httpCacheManager.setQuotaBytes(MEDIUM);
                    httpCacheManager.setQuotaBytes(Integer.MAX_VALUE + 1L);
                    Assert.assertEquals(MAXIMUM, httpCacheManager.getQuotaBytes());

                    httpCacheManager.setQuotaBytes(MEDIUM);
                    httpCacheManager.setQuotaBytes(Long.MAX_VALUE);
                    Assert.assertEquals(MAXIMUM, httpCacheManager.getQuotaBytes());
                });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testIllegalValues() throws Throwable {
        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwHttpCacheManager httpCacheManager = profile.getHttpCacheManager();

                    Assert.assertThrows(
                            IllegalArgumentException.class,
                            () -> {
                                httpCacheManager.setQuotaBytes(-1);
                            });
                    Assert.assertTrue(httpCacheManager.isUsingDefaultQuota());

                    Assert.assertThrows(
                            IllegalArgumentException.class,
                            () -> {
                                httpCacheManager.setQuotaBytes(Integer.MIN_VALUE);
                            });
                    Assert.assertTrue(httpCacheManager.isUsingDefaultQuota());

                    Assert.assertThrows(
                            IllegalArgumentException.class,
                            () -> {
                                httpCacheManager.setQuotaBytes(Integer.MIN_VALUE - 1L);
                            });
                    Assert.assertTrue(httpCacheManager.isUsingDefaultQuota());

                    Assert.assertThrows(
                            IllegalArgumentException.class,
                            () -> {
                                httpCacheManager.setQuotaBytes(Long.MIN_VALUE);
                            });
                    Assert.assertTrue(httpCacheManager.isUsingDefaultQuota());
                });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testHistograms() throws Throwable {
        final long quota1 = 10 * 1024 * 1024;
        final long quota2 = 20 * 1024 * 1024;

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Android.WebView.HttpCacheQuotaApi.RestoredFromPrefs.NonDefault",
                                false)
                        .build();
        AwBrowserContext profile = mRule.getProfileSync(PROFILE_NAME, true);
        watcher.assertExpected();

        AwHttpCacheManager httpCacheManager =
                ThreadUtils.runOnUiThreadBlocking(() -> profile.getHttpCacheManager());

        final long defaultQuota =
                ThreadUtils.runOnUiThreadBlocking(() -> httpCacheManager.getDefaultQuotaBytes());
        final int expectedAbsolute1 = (int) (quota1 / 1024);
        final int expectedRelative1 = (int) (quota1 * 100.0f / defaultQuota);
        final int expectedAbsolute2 = (int) (quota2 / 1024);
        final int expectedRelative2 = (int) (quota2 * 100.0f / defaultQuota);

        // Do not test HttpCache.SetMaxBytes.BackendStartedOrStarting=false, as backends may be
        // initialized eagerly or lazily.

        // 1. First call to setQuotaBytes
        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.WebView.HttpCacheQuotaApi.AbsoluteValue.NonDefault",
                                expectedAbsolute1)
                        .expectIntRecord(
                                "Android.WebView.HttpCacheQuotaApi.RelativeValue.NonDefault",
                                expectedRelative1)
                        .expectIntRecord(
                                "Android.WebView.HttpCacheQuotaApi.ChangeHistory.NonDefault",
                                0) // kFirstTime
                        .build();
        ThreadUtils.runOnUiThreadBlocking(() -> httpCacheManager.setQuotaBytes(quota1));
        watcher.assertExpected();

        AwContents awContents = mRule.createAwContents(profile);
        String url = mWebServer.setResponse("/", "This forces cache initialization", null);
        mRule.loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), url);

        // 2. Repeated call to setQuotaBytes without change
        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.WebView.HttpCacheQuotaApi.AbsoluteValue.NonDefault",
                                expectedAbsolute1)
                        .expectIntRecord(
                                "Android.WebView.HttpCacheQuotaApi.RelativeValue.NonDefault",
                                expectedRelative1)
                        .expectIntRecord(
                                "Android.WebView.HttpCacheQuotaApi.ChangeHistory.NonDefault",
                                1) // kRepeatedCallWithoutChange
                        .build();
        ThreadUtils.runOnUiThreadBlocking(() -> httpCacheManager.setQuotaBytes(quota1));
        watcher.assertExpected();

        // 3. Repeated call to setQuotaBytes with change
        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.WebView.HttpCacheQuotaApi.AbsoluteValue.NonDefault",
                                expectedAbsolute2)
                        .expectIntRecord(
                                "Android.WebView.HttpCacheQuotaApi.RelativeValue.NonDefault",
                                expectedRelative2)
                        .expectIntRecord(
                                "Android.WebView.HttpCacheQuotaApi.ChangeHistory.NonDefault",
                                2) // kRepeatedCallWithChange
                        .build();
        ThreadUtils.runOnUiThreadBlocking(() -> httpCacheManager.setQuotaBytes(quota2));
        watcher.assertExpected();

        // 4. Call to useDefaultHttpCacheQuota (changes back to default, so it's a change)
        // Note that it's a change even if the default happens to match the earlier set value.
        watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Android.WebView.HttpCacheQuotaApi.UseDefaultQuota.NonDefault",
                                true)
                        .expectIntRecord(
                                "Android.WebView.HttpCacheQuotaApi.ChangeHistory.NonDefault",
                                2) // kRepeatedCallWithChange
                        .build();
        ThreadUtils.runOnUiThreadBlocking(() -> httpCacheManager.useDefaultQuota());
        watcher.assertExpected();

        // 5. Repeated call to useDefaultQuota without change
        watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Android.WebView.HttpCacheQuotaApi.UseDefaultQuota.NonDefault",
                                true)
                        .expectIntRecord(
                                "Android.WebView.HttpCacheQuotaApi.ChangeHistory.NonDefault",
                                1) // kRepeatedCallWithoutChange
                        .build();
        ThreadUtils.runOnUiThreadBlocking(() -> httpCacheManager.useDefaultQuota());
        watcher.assertExpected();
    }
}
