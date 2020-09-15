// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.Robolectric;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.test.util.browser.Features;

import java.io.IOException;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.List;

/** Tests for {@link TrendyTermsCache}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrendyTermsCacheUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private EndpointFetcher.Natives mMockNatives;
    @Mock
    Profile mProfile;
    @Captor
    ArgumentCaptor<Callback<EndpointResponse>> mCallbackCaptor;

    @Spy
    TrendyTermsCache mCache = new TrendyTermsCache();

    private String mRss = "<title>trends</title>\n"
            + "  <title> term</title>\n"
            + "  <title>term2 </title>\n";

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(EndpointFetcherJni.TEST_HOOKS, mMockNatives);
        TrendyTermsCache.setInstanceForTesting(mCache);
        StartSurfaceConfiguration.TRENDY_ENABLED.setForTesting(true);
        PostTask.setPrenativeThreadPoolExecutorForTesting(new RoboExecutorService());
    }

    @After
    public void tearDown() {
        TrendyTermsCache.setInstanceForTesting(null);
        PostTask.resetPrenativeThreadPoolExecutorForTesting();
        TrendyTermsCache.getSharedPreferences().edit().clear().commit();
    }

    @Test
    public void testMaybeFetch_NotFetching() {
        doReturn(false).when(mCache).shouldFetch();

        TrendyTermsCache.maybeFetch(mProfile);
        Robolectric.flushBackgroundThreadScheduler();
        Robolectric.flushForegroundThreadScheduler();
        verify(mMockNatives, never()).nativeFetchWithNoAuth(any(), any(), any());
    }

    @Test
    public void testMaybeFetch_FetchingFailed() {
        doReturn(123L).when(mCache).getCurrentTime();
        doReturn(true).when(mCache).shouldFetch();

        Assert.assertEquals(-1,
                TrendyTermsCache.getSharedPreferences().getLong(
                        TrendyTermsCache.SUPPRESSED_UNTIL_KEY, -1));

        TrendyTermsCache.maybeFetch(mProfile);
        Robolectric.flushBackgroundThreadScheduler();
        Robolectric.flushForegroundThreadScheduler();
        verify(mMockNatives).nativeFetchWithNoAuth(any(), any(), mCallbackCaptor.capture());
        Callback<EndpointResponse> callback = mCallbackCaptor.getValue();
        callback.onResult(null);
        Assert.assertEquals(7200_123,
                TrendyTermsCache.getSharedPreferences().getLong(
                        TrendyTermsCache.SUPPRESSED_UNTIL_KEY, -1));
    }

    @Test
    public void testMaybeFetch_FetchingMalformed() {
        doReturn(123L).when(mCache).getCurrentTime();
        doReturn(true).when(mCache).shouldFetch();

        Assert.assertEquals(-1,
                TrendyTermsCache.getSharedPreferences().getLong(
                        TrendyTermsCache.SUPPRESSED_UNTIL_KEY, -1));

        TrendyTermsCache.maybeFetch(mProfile);
        Robolectric.flushBackgroundThreadScheduler();
        Robolectric.flushForegroundThreadScheduler();
        verify(mMockNatives).nativeFetchWithNoAuth(any(), any(), mCallbackCaptor.capture());
        Callback<EndpointResponse> callback = mCallbackCaptor.getValue();
        callback.onResult(new EndpointResponse(""));
        Assert.assertEquals(7200_123,
                TrendyTermsCache.getSharedPreferences().getLong(
                        TrendyTermsCache.SUPPRESSED_UNTIL_KEY, -1));
    }

    @Test
    public void testMaybeFetch_FetchingSucceeded() {
        doReturn(123L).when(mCache).getCurrentTime();
        doReturn(true).when(mCache).shouldFetch();
        Assert.assertEquals(-1,
                TrendyTermsCache.getSharedPreferences().getLong(
                        TrendyTermsCache.SUPPRESSED_UNTIL_KEY, -1));

        TrendyTermsCache.maybeFetch(mProfile);
        Robolectric.flushBackgroundThreadScheduler();
        Robolectric.flushForegroundThreadScheduler();
        verify(mMockNatives).nativeFetchWithNoAuth(any(), any(), mCallbackCaptor.capture());
        Callback<EndpointResponse> callback = mCallbackCaptor.getValue();
        callback.onResult(new EndpointResponse(mRss));
        Assert.assertEquals(86400_123,
                TrendyTermsCache.getSharedPreferences().getLong(
                        TrendyTermsCache.SUPPRESSED_UNTIL_KEY, -1));
    }

    @Test
    public void testShouldFetch_Unlimited() {
        doReturn(123L).when(mCache).getCurrentTime();
        TrendyTermsCache.getSharedPreferences()
                .edit()
                .putLong(TrendyTermsCache.SUPPRESSED_UNTIL_KEY, 122)
                .commit();

        Assert.assertTrue(mCache.shouldFetch());
    }

    @Test
    public void testShouldFetch_RateLimited() {
        doReturn(123L).when(mCache).getCurrentTime();
        TrendyTermsCache.getSharedPreferences()
                .edit()
                .putLong(TrendyTermsCache.SUPPRESSED_UNTIL_KEY, 124)
                .commit();

        Assert.assertFalse(mCache.shouldFetch());
    }

    @Test
    public void testSaveAndGet() {
        List<String> terms = new ArrayList<>();
        Assert.assertEquals(terms, TrendyTermsCache.getTrendyTerms());

        terms.add("test");
        TrendyTermsCache.saveTrendyTerms(terms);
        Assert.assertEquals(terms, TrendyTermsCache.getTrendyTerms());

        terms.add("another");
        TrendyTermsCache.saveTrendyTerms(terms);
        Assert.assertEquals(terms, TrendyTermsCache.getTrendyTerms());
    }

    @Test
    public void testParseRSS() throws IOException {
        List<String> terms = new ArrayList<>();
        terms.add("term");
        terms.add("term2");
        Assert.assertEquals(terms, TrendyTermsCache.parseRSS(new StringReader(mRss)));
    }
}
