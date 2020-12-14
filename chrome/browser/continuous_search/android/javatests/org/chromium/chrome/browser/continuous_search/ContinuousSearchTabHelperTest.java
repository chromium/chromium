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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
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
                mListener.onResult(new SearchResultMetadata(
                        mSearchUrl, mQuery, 0, new ArrayList<SearchResultGroup>()));
            }, 50);
        }

        @Override
        public void cancel() {}
    }

    /**
     * A {@link SearchResultUserDataObserver} used to wait on events.
     */
    public class WaitableSearchResultUserDataObserver implements SearchResultUserDataObserver {
        public CallbackHelper mInvalidateCallbackHelper = new CallbackHelper();
        public CallbackHelper mOnUpdateCallbackHelper = new CallbackHelper();
        public SearchResultMetadata mMetadata;
        public GURL mUrl;

        @Override
        public void onInvalidate() {
            mInvalidateCallbackHelper.notifyCalled();
        }

        @Override
        public void onUpdate(SearchResultMetadata metadata, GURL url) {
            mMetadata = metadata;
            mUrl = url;
            mOnUpdateCallbackHelper.notifyCalled();
        }

        @Override
        public void onUrlChanged(GURL url) {
            mUrl = url;
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

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1157325")
    public void testContinuousSearchFakeResults() throws TimeoutException {
        WaitableSearchResultUserDataObserver observer = new WaitableSearchResultUserDataObserver();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            SearchResultUserData searchResultUserData = SearchResultUserData.getForTab(tab);
            Assert.assertNotNull(searchResultUserData);
            searchResultUserData.addObserver(observer);
            tab.loadUrl(new LoadUrlParams(
                    mServer.getURLWithHostName("www.google.com", TEST_URL + "?q=cat+dog")));
        });

        observer.mOnUpdateCallbackHelper.waitForFirst(5000, TimeUnit.MILLISECONDS);
        Assert.assertEquals("cat dog", observer.mMetadata.getQuery());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            SearchResultUserData searchResultUserData = SearchResultUserData.getForTab(tab);
            Assert.assertTrue(searchResultUserData.isValid());
            String url = mServer.getURLWithHostName("www.google.com", TEST_URL + "?q=cat+dog");
            Assert.assertTrue(observer.mMetadata.getResultUrl().getSpec().startsWith(url));
            Assert.assertTrue(observer.mUrl.getSpec().startsWith(url));
            tab.loadUrl(new LoadUrlParams(UrlConstants.ABOUT_URL));
        });

        observer.mInvalidateCallbackHelper.waitForFirst(5000, TimeUnit.MILLISECONDS);
    }
}
