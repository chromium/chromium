// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.webkit.WebSettings;

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
import org.chromium.android_webview.AwServiceWorkerSettings;
import org.chromium.android_webview.ManifestMetadataUtil;
import org.chromium.base.Log;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.util.Collections;
import java.util.Set;

/**
 * Test service worker settings APIs.
 *
 * These tests are functionally duplicates of the ones in {@link AwSettingsTest},
 * and serve to ensure that service worker settings are applied, even if no
 * {@link android.webkit.ServiceWorkerClient} is supplied.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwServiceWorkerSettingsTest extends AwParameterizedTest {
    public static final String TAG = "AwSWSettingsTest";
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestWebServer mWebServer;

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;

    private AwServiceWorkerSettings mAwServiceWorkerSettings;

    public static final String INDEX_URL = "/index.html";
    public static final String SW_URL = "/sw.js";
    public static final String FETCH_URL = "/content.txt";
    private static final String INDEX_HTML_TEMPLATE =
            "<!DOCTYPE html>\n"
                    + "<script>\n"
                    + "    state = '';\n"
                    + "    function setState(newState) {\n"
                    + "        console.log(newState);\n"
                    + "        state = newState;\n"
                    + "    }\n"
                    + "    function swReady(sw) {\n"
                    + "        setState('sw_ready');\n"
                    + "        sw.postMessage({fetches: %d});\n" // <- Format param on this line
                    + "    }\n"
                    + "    navigator.serviceWorker.register('sw.js')\n"
                    + "        .then(sw_reg => {\n"
                    + "            setState('sw_registered');\n"
                    + "            let sw = sw_reg.installing || sw_reg.waiting || sw_reg.active;\n"
                    + "            if (sw.state == 'activated') {\n"
                    + "                swReady(sw);\n"
                    + "            } else {\n"
                    + "                sw.addEventListener('statechange', e => {\n"
                    + "                    if(e.target.state == 'activated') swReady(e.target); \n"
                    + "                });            \n"
                    + "            }\n"
                    + "        }).catch(err => {\n"
                    + "            console.log(err);\n"
                    + "            setState('sw_registration_error');\n"
                    + "        });\n"
                    + "    navigator.serviceWorker.addEventListener('message',\n"
                    + "        event => setState(event.data.msg));\n"
                    + "    setState('page_loaded');\n"
                    + "</script>\n";

    private static final String NETWORK_ACCESS_SW_JS =
            "self.addEventListener('message', async event => {\n"
                    + "    try {\n"
                    + "        let resp;\n"
                    + "        for (let i = 0; i < event.data.fetches; i++) {\n"
                    + "            resp = await fetch('content.txt');\n"
                    + "        }\n"
                    + "        if (resp && resp.ok) {\n"
                    + "            event.source.postMessage({ msg: await resp.text() });\n"
                    + "        } else {\n"
                    + "            event.source.postMessage({ msg: 'fetch_not_ok' });\n"
                    + "        }\n"
                    + "    } catch {\n"
                    + "        event.source.postMessage({ msg: 'fetch_catch' })\n"
                    + "    }\n"
                    + "});\n";

    private static final String FETCH_CONTENT = "fetch_success";

    public AwServiceWorkerSettingsTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
    }

    /**
     * Initialize test fields.
     * Extracted to separate method instead of {@code setUp} to allow certain tests
     * to configure an ApplicationContext before startup
     */
    private void initAwServiceWorkerSettings() {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mAwServiceWorkerSettings =
                mActivityTestRule
                        .getAwBrowserContext()
                        .getServiceWorkerController()
                        .getAwServiceWorkerSettings();

        // To ensure that any settings supplied by the user are respected, even if the
        // serviceWorkerClient is null, we set it explicitly here.
        // See http://crbug.com/979321
        mActivityTestRule
                .getAwBrowserContext()
                .getServiceWorkerController()
                .setServiceWorkerClient(null);
    }

    @After
    public void tearDown() {
        if (mWebServer != null) mWebServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "ServiceWorker"})
    public void testBlockNetworkLoadsFalse() throws Throwable {
        initAwServiceWorkerSettings();
        final String fullIndexUrl = mWebServer.setResponse(INDEX_URL, indexHtml(1), null);
        mWebServer.setResponse(SW_URL, NETWORK_ACCESS_SW_JS, null);
        mWebServer.setResponse(FETCH_URL, FETCH_CONTENT, null);

        mAwServiceWorkerSettings.setBlockNetworkLoads(false);

        loadPage(fullIndexUrl, FETCH_CONTENT);
        Assert.assertEquals(1, mWebServer.getRequestCount(SW_URL));
        Assert.assertEquals(
                "The service worker should make one network request",
                1,
                mWebServer.getRequestCount(FETCH_URL));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "ServiceWorker"})
    public void testBlockNetworkLoadsTrue() throws Throwable {
        initAwServiceWorkerSettings();
        final String fullIndexUrl = mWebServer.setResponse(INDEX_URL, indexHtml(1), null);
        mWebServer.setResponse(SW_URL, NETWORK_ACCESS_SW_JS, null);
        mWebServer.setResponse(FETCH_URL, FETCH_CONTENT, null);

        mAwServiceWorkerSettings.setBlockNetworkLoads(true);

        // With network turned off, we do not expect to be able to load the sw.js, and registration
        // will fail
        loadPage(fullIndexUrl, "sw_registration_error");
        Assert.assertEquals(
                "The service worker should not be loaded", 0, mWebServer.getRequestCount(SW_URL));
        Assert.assertEquals(
                "The service worker should not make any network requests",
                0,
                mWebServer.getRequestCount(FETCH_URL));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "ServiceWorker"})
    public void testCacheModeLoadNoCache() throws Throwable {
        initAwServiceWorkerSettings();
        final String fullIndexUrl = mWebServer.setResponse(INDEX_URL, indexHtml(2), null);
        mWebServer.setResponse(SW_URL, NETWORK_ACCESS_SW_JS, null);
        mWebServer.setResponse(FETCH_URL, FETCH_CONTENT, null);

        mAwServiceWorkerSettings.setCacheMode(WebSettings.LOAD_NO_CACHE);

        loadPage(fullIndexUrl, FETCH_CONTENT);
        Assert.assertEquals(
                "Two requests should be made in no-cache mode",
                2,
                mWebServer.getRequestCount(FETCH_URL));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "ServiceWorker"})
    public void testCacheModeLoadCacheElseNetwork() throws Throwable {
        initAwServiceWorkerSettings();
        final String fullIndexUrl = mWebServer.setResponse(INDEX_URL, indexHtml(2), null);
        mWebServer.setResponse(SW_URL, NETWORK_ACCESS_SW_JS, null);
        mWebServer.setResponse(FETCH_URL, FETCH_CONTENT, null);

        mAwServiceWorkerSettings.setCacheMode(WebSettings.LOAD_CACHE_ELSE_NETWORK);

        loadPage(fullIndexUrl, FETCH_CONTENT);
        Assert.assertEquals(
                "Only one request should be made when cache is available",
                1,
                mWebServer.getRequestCount(FETCH_URL));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "ServiceWorker"})
    public void testCacheModeLoadCacheOnly() throws Throwable {
        initAwServiceWorkerSettings();
        final String fullIndexUrl = mWebServer.setResponse(INDEX_URL, indexHtml(2), null);
        mWebServer.setResponse(SW_URL, NETWORK_ACCESS_SW_JS, null);
        mWebServer.setResponse(FETCH_URL, FETCH_CONTENT, null);

        mAwServiceWorkerSettings.setCacheMode(WebSettings.LOAD_CACHE_ONLY);

        // sw won't be in cache so register will fail
        loadPage(fullIndexUrl, "sw_registration_error");
        Assert.assertEquals(
                "No requests should be made in cache-only mode",
                0,
                mWebServer.getRequestCount(SW_URL));
        Assert.assertEquals(
                "No requests should be made in cache-only mode",
                0,
                mWebServer.getRequestCount(FETCH_URL));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "ServiceWorker"})
    public void testGetUpdatedXRWAllowList() throws Throwable {
        initAwServiceWorkerSettings();
        final Set<String> allowList = Set.of("https://*.example.com", "https://*.google.com");

        Assert.assertEquals(
                Collections.emptySet(),
                mAwServiceWorkerSettings.getRequestedWithHeaderOriginAllowList());

        mAwServiceWorkerSettings.setRequestedWithHeaderOriginAllowList(allowList);

        Assert.assertEquals(
                allowList, mAwServiceWorkerSettings.getRequestedWithHeaderOriginAllowList());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "ServiceWorker"})
    public void testXRequestedWithAllowListSetByManifest() throws Throwable {
        final Set<String> allowList = Set.of("https://*.example.com", "https://*.google.com");
        try (var a = ManifestMetadataUtil.setXRequestedWithAllowListScopedForTesting(allowList)) {
            // Only initialize once the manifest has been configured
            initAwServiceWorkerSettings();

            Set<String> changedList =
                    mAwServiceWorkerSettings.getRequestedWithHeaderOriginAllowList();
            Assert.assertEquals(allowList, changedList);
        }
    }

    private String indexHtml(int fetches) {
        return String.format(INDEX_HTML_TEMPLATE, fetches);
    }

    private void loadPage(final String fullIndexUrl, String expectedState) throws Exception {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, fullIndexUrl);
        Assert.assertEquals(fullIndexUrl, onPageFinishedHelper.getUrl());
        String expectedAsJson = "\"" + expectedState + "\"";
        AwActivityTestRule.pollInstrumentationThread(() -> expectedAsJson.equals(getStateFromJs()));
    }

    private String getStateFromJs() throws Exception {
        String state =
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "state");
        // Logging the state helps with troubleshooting
        Log.i(TAG, "state = %s", state);
        return state;
    }
}
