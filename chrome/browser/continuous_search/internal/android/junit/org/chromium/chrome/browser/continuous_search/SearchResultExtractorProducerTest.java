// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.continuous_search.SearchResultExtractorClientStatus;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/**
 * Test of {@link SearchResultExtractorProducer}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchResultExtractorProducerTest {
    private static final long FAKE_NATIVE_ADDRESS = 0x7489;
    private static final String TEST_QUERY = "Bar";
    private static final int TEST_RESULT_TYPE = 1;

    private SearchResultExtractorProducer mSearchResultProducer;
    @Mock
    private Tab mTabMock;
    @Mock
    private WebContents mWebContentsMock;
    @Mock
    private SearchResultListener mListenerMock;
    @Mock
    private SearchResultExtractorProducer.Natives mSearchResultExtractorProducerJniMock;
    private GURL mTestUrl;

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Before
    public void setUp() {
        mTestUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        initMocks(this);
        mJniMocker.mock(
                SearchResultExtractorProducerJni.TEST_HOOKS, mSearchResultExtractorProducerJniMock);
        when(mSearchResultExtractorProducerJniMock.create(any())).thenReturn(FAKE_NATIVE_ADDRESS);
        mSearchResultProducer = new SearchResultExtractorProducer(mTabMock, mListenerMock);
        when(mTabMock.getWebContents()).thenReturn(mWebContentsMock);
        when(mWebContentsMock.getLastCommittedUrl()).thenReturn(mTestUrl);
    }

    /**
     * Starts fetching data.
     */
    private void startFetching() {
        mSearchResultProducer.fetchResults(mTestUrl, TEST_QUERY);

        verify(mSearchResultExtractorProducerJniMock, times(1))
                .fetchResults(FAKE_NATIVE_ADDRESS, mWebContentsMock, TEST_QUERY);
    }

    /**
     * Finishes fetching data.
     * @param cancelled Whether to treat the fetch as if it was cancelled.
     */
    private void finishFetching(boolean cancelled) {
        GURL url1 = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);
        GURL url2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1);
        GURL url3 = JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_2);
        GURL url4 = JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_3);

        mSearchResultProducer.onResultsAvailable(mTestUrl, TEST_QUERY, TEST_RESULT_TYPE,
                new String[] {"Foo", "Bar"}, new boolean[] {false, true}, new int[] {1, 3},
                new String[] {"Foo.com 1", "Bar.com 1", "Bar.com 2", "Bar.com 3"},
                new GURL[] {url1, url2, url3, url4});

        if (cancelled) {
            verify(mListenerMock, never()).onResult(any());
            return;
        }

        List<PageGroup> groups = new ArrayList<PageGroup>();
        List<PageItem> results1 = new ArrayList<PageItem>();
        results1.add(new PageItem(url1, "Foo.com 1"));
        groups.add(new PageGroup("Foo", false, results1));
        List<PageItem> results2 = new ArrayList<PageItem>();
        results2.add(new PageItem(url2, "Bar.com 1"));
        results2.add(new PageItem(url3, "Bar.com 2"));
        results2.add(new PageItem(url4, "Bar.com 3"));
        groups.add(new PageGroup("Bar", true, results2));

        verify(mListenerMock, times(1))
                .onResult(new ContinuousNavigationMetadata(
                        mTestUrl, TEST_QUERY, TEST_RESULT_TYPE, groups));
    }

    /**
     * Fetches data.
     */
    private void fetchResultsSuccessfully() {
        startFetching();
        finishFetching(false);
    }

    /**
     * Test a successful fetch.
     */
    @Test
    public void testFetchResultsSuccess() {
        fetchResultsSuccessfully();
    }

    /**
     * Ensure two parallel fetches can't occur.
     */
    @Test
    public void testFetchResultsInParallel() {
        startFetching();

        // Trying to capture while a request is ongoing shouldn't succeed.
        mSearchResultProducer.fetchResults(mTestUrl, TEST_QUERY);
        verify(mListenerMock, times(1))
                .onError(SearchResultExtractorClientStatus.ALREADY_CAPTURING);

        finishFetching(false);
    }

    /**
     * Ensure two parallel fetches can't occur even if the first is cancelled, until the first fetch
     * resolves.
     */
    @Test
    public void testFetchResultsInParallelWithCancel() {
        startFetching();
        mSearchResultProducer.cancel();

        // Trying to capture while a cancelled request is ongoing shouldn't succeed.
        mSearchResultProducer.fetchResults(mTestUrl, TEST_QUERY);
        verify(mListenerMock, times(1))
                .onError(SearchResultExtractorClientStatus.ALREADY_CAPTURING);

        finishFetching(true);

        reset(mListenerMock);
        reset(mSearchResultExtractorProducerJniMock);
        fetchResultsSuccessfully();
    }

    /**
     * Ensure no attempt to fetch is made if the web contents doesn't exist.
     */
    @Test
    public void testFetchResultsNoWebContents() {
        reset(mTabMock);
        mSearchResultProducer.fetchResults(mTestUrl, TEST_QUERY);

        verify(mListenerMock, times(1))
                .onError(SearchResultExtractorClientStatus.WEB_CONTENTS_GONE);
        verify(mSearchResultExtractorProducerJniMock, never())
                .fetchResults(anyLong(), any(), any());
    }

    /**
     * Ensure no attempt to fetch is made if the web contents url doesn't match the provided URL.
     */
    @Test
    public void testFetchResultsWebContentsUrlMismatch() {
        reset(mWebContentsMock);
        when(mWebContentsMock.getLastCommittedUrl())
                .thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1));
        mSearchResultProducer.fetchResults(mTestUrl, TEST_QUERY);

        verify(mListenerMock, times(1)).onError(SearchResultExtractorClientStatus.UNEXPECTED_URL);
        verify(mSearchResultExtractorProducerJniMock, never())
                .fetchResults(anyLong(), any(), any());
    }

    /**
     * Ensure no attempt to fetch is made if native doesn't exist.
     */
    @Test
    public void testFetchResultsNoNative() {
        mSearchResultProducer.destroy();
        mSearchResultProducer.fetchResults(mTestUrl, TEST_QUERY);

        verify(mSearchResultExtractorProducerJniMock, times(1)).destroy(FAKE_NATIVE_ADDRESS);
        verify(mListenerMock, times(1))
                .onError(SearchResultExtractorClientStatus.NATIVE_NOT_INITIALIZED);
        verify(mSearchResultExtractorProducerJniMock, never())
                .fetchResults(anyLong(), any(), any());
    }

    /**
     * Verify that results can be requested successfully if a pre-emptive cancel is issued.
     */
    @Test
    public void testFetchResultsAfterCancel() {
        mSearchResultProducer.cancel();
        fetchResultsSuccessfully();
    }

    /**
     * Verify that results are not posted if the request is cancelled, but that subsequent requests
     * succeed.
     */
    @Test
    public void testFetchResultsCancelled() {
        startFetching();
        mSearchResultProducer.cancel();
        finishFetching(true);

        reset(mListenerMock);
        reset(mSearchResultExtractorProducerJniMock);
        fetchResultsSuccessfully();
    }

    /**
     * Verify calling destroy twice again doesn't crash. This shouldn't happen, but this is a
     * precaution.
     */
    @Test
    public void testSafeDestroy() {
        mSearchResultProducer.destroy();
        mSearchResultProducer.destroy();
        verify(mSearchResultExtractorProducerJniMock, times(1)).destroy(FAKE_NATIVE_ADDRESS);
    }

    /**
     * Verify that results can be requested successfully if a pre-emptive cancel is issued.
     */
    @Test
    public void testOnError() {
        startFetching();
        mSearchResultProducer.onError(SearchResultExtractorClientStatus.WEB_CONTENTS_GONE);
        verify(mListenerMock, times(1))
                .onError(SearchResultExtractorClientStatus.WEB_CONTENTS_GONE);

        reset(mListenerMock);
        reset(mSearchResultExtractorProducerJniMock);
        fetchResultsSuccessfully();
    }

    /**
     * Verify that results can be requested successfully if a pre-emptive cancel is issued.
     */
    @Test
    public void testOnErrorNoOpOnCancel() {
        startFetching();
        mSearchResultProducer.cancel();
        mSearchResultProducer.onError(SearchResultExtractorClientStatus.WEB_CONTENTS_GONE);
        verify(mListenerMock, never()).onError(anyInt());
    }
}
