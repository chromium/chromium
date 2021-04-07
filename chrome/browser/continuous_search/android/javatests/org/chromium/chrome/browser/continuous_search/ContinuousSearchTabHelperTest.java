// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.os.Handler;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the {@link ContinuousSearchTabHelper} class.
 *
 * To serve a https://www.google.com/search?q= URL we need to set flags so that the {@link
 * EmbeddedTestServer} serves the files properly and {@link SearchUrlHelper} can work without
 * being mocked. It is also necessary to ignore port numbers as only 80 and 443 are expected, but
 * the {@link EmbeddedTestServer} will use any open port.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures({ChromeFeatureList.CONTINUOUS_SEARCH})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "ignore-google-port-numbers",
        "ignore-certificate-errors", ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
@Batch(PER_CLASS)
public class ContinuousSearchTabHelperTest {
    private static final String TEST_SERVER_DIR = "chrome/browser/continuous_search/testdata";
    private static final String TEST_URL = "/search";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mServer;

    /**
     * Fake implementation of {@link SearchResultProducer} that returns the data passed to it and no
     * results.
     */
    public class FakeSearchResultProducer extends SearchResultProducer {
        private String mQuery;
        private GURL mSearchUrl;

        FakeSearchResultProducer(Tab tab, SearchResultListener listener) {
            super(tab, listener);
        }

        @Override
        public void fetchResults(GURL url, String query) {
            mSearchUrl = url;
            mQuery = query;
            new Handler().postDelayed(() -> {
                mListener.onResult(new ContinuousNavigationMetadata(
                        mSearchUrl, mQuery, 0, new ArrayList<PageGroup>()));
            }, 300);
        }

        @Override
        public void cancel() {}
    }

    /**
     * A {@link ContinuousNavigationUserDataObserver} used to wait on events.
     */
    public class WaitableContinuousNavigationUserDataObserver
            implements ContinuousNavigationUserDataObserver {
        public CallbackHelper mInvalidateCallbackHelper = new CallbackHelper();
        public CallbackHelper mOnUpdateCallbackHelper = new CallbackHelper();
        public ContinuousNavigationMetadata mMetadata;
        public GURL mUrl;
        public boolean mOnSrp;

        @Override
        public void onInvalidate() {
            mInvalidateCallbackHelper.notifyCalled();
        }

        @Override
        public void onUpdate(ContinuousNavigationMetadata metadata) {
            mMetadata = metadata;
            mOnUpdateCallbackHelper.notifyCalled();
        }

        @Override
        public void onUrlChanged(GURL url, boolean onSrp) {
            mUrl = url;
            mOnSrp = onSrp;
        }
    }

    @Before
    public void setUp() {
        SearchResultProducerFactory.overrideFactory((Tab tab, SearchResultListener listener) -> {
            return new FakeSearchResultProducer(tab, listener);
        });
        mActivityTestRule.startMainActivityOnBlankPage();
        mServer = new EmbeddedTestServer();
        mServer.initializeNative(InstrumentationRegistry.getContext(),
                EmbeddedTestServer.ServerHTTPSSetting.USE_HTTPS);
        mServer.addDefaultHandlers(TEST_SERVER_DIR);
        mServer.start();
    }

    @After
    public void tearDown() {
        mServer.stopAndDestroyServer();
    }

    /**
     * Loads a tab with a provided set of params. Will exit via {@link Assert#fail()} if the tab
     * fails to load, crashes, or exceeds the a certain timeout window. Otherwise, the tab will
     * be successfully loaded upon returning.
     * @param tab The tab to load.
     * @param params The URL and loading type for the tab.
     */
    private void loadUrl(Tab tab, LoadUrlParams params) {
        final CallbackHelper startedCallback = new CallbackHelper();
        final CallbackHelper loadedCallback = new CallbackHelper();
        final CallbackHelper failedCallback = new CallbackHelper();
        final CallbackHelper crashedCallback = new CallbackHelper();

        TabObserver observer = new EmptyTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                startedCallback.notifyCalled();
            }

            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                loadedCallback.notifyCalled();
            }

            @Override
            public void onPageLoadFailed(Tab tab, int errorCode) {
                failedCallback.notifyCalled();
            }

            @Override
            public void onCrash(Tab tab) {
                crashedCallback.notifyCalled();
            }
        };
        tab.addObserver(observer);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> { tab.loadUrl(params); });

        // Ensure the tab loads.
        try {
            startedCallback.waitForCallback(0, 1);
        } catch (TimeoutException e) {
            Assert.fail("Tab never started loading.");
        }
        boolean timedOut = false;
        try {
            loadedCallback.waitForCallback(0, 1);
        } catch (TimeoutException e) {
            timedOut = true;
        }

        // If the tab doesn't fully load, try to determine what happened for easier debugging.
        if (timedOut) {
            try {
                failedCallback.waitForCallback(0, 1);
                Assert.fail("Tab failed to load.");
            } catch (TimeoutException e) {
                // Tab didn't fail to load so continue.
            }
            try {
                crashedCallback.waitForCallback(0, 1);
                Assert.fail("Tab crashed while loading.");
            } catch (TimeoutException e) {
                // Tab didn't crash so continue.
            }
            Assert.fail("Tab timed out while loading.");
        }

        tab.removeObserver(observer);
    }

    @Test
    @MediumTest
    public void testContinuousSearchFakeResults() throws TimeoutException {
        WaitableContinuousNavigationUserDataObserver observer =
                new WaitableContinuousNavigationUserDataObserver();

        // Load a SRP URL.
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            ContinuousNavigationUserDataImpl continuousNavigationUserData =
                    ContinuousNavigationUserDataImpl.getOrCreateForTab(tab);
            Assert.assertNotNull(continuousNavigationUserData);
            continuousNavigationUserData.addObserver(observer);
        });
        loadUrl(tab,
                new LoadUrlParams(
                        mServer.getURLWithHostName("www.google.com", TEST_URL + "?q=cat+dog")));
        observer.mOnUpdateCallbackHelper.waitForFirst(
                "Timed out waiting for SearchResultUserDataObserver#onUpdate", 5000,
                TimeUnit.MILLISECONDS);

        // Check the retuned data.
        Assert.assertEquals("cat dog", observer.mMetadata.getQuery());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            ContinuousNavigationUserDataImpl continuousNavigationUserData =
                    ContinuousNavigationUserDataImpl.getOrCreateForTab(tab);
            Assert.assertTrue(continuousNavigationUserData.isValid());
            String url = mServer.getURLWithHostName("www.google.com", TEST_URL + "?q=cat+dog");
            Assert.assertTrue(observer.mMetadata.getRootUrl().getSpec().startsWith(url));
            Assert.assertTrue(observer.mUrl.getSpec().startsWith(url));
            Assert.assertTrue(observer.mOnSrp);
        });

        // Invalidate the data.
        loadUrl(tab, new LoadUrlParams(UrlConstants.ABOUT_URL));
        observer.mInvalidateCallbackHelper.waitForFirst(
                "Timed out waiting for SearchResultUserDataObserver#onInvalidate", 5000,
                TimeUnit.MILLISECONDS);
    }
}
