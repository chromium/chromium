// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Tests for {@link ContinuousSearchTabObserver}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ContinuousSearchTabObserverTest {
    private static final String TEST_QUERY = "Foo";

    @Mock
    private Tab mTabMock;
    @Mock
    private SearchUrlHelper.Natives mSearchUrlHelperJniMock;
    @Mock
    private ContinuousNavigationUserDataImpl mUserDataMock;
    @Mock
    private SearchResultProducer mProducerMock;

    private GURL mSrpUrl;
    private GURL mNonSrpUrl;
    private ContinuousSearchTabObserver mObserver;

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    class FakeSearchResultProducerFactory
            implements SearchResultProducerFactory.SearchResultProducerFactoryImpl {
        @Override
        public SearchResultProducer create(Tab tab, SearchResultListener listener) {
            return mProducerMock;
        }
    }

    @Before
    public void setUp() {
        mSrpUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
        mNonSrpUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);

        initMocks(this);
        mJniMocker.mock(SearchUrlHelperJni.TEST_HOOKS, mSearchUrlHelperJniMock);
        doReturn(TEST_QUERY).when(mSearchUrlHelperJniMock).getQueryIfValidSrpUrl(eq(mSrpUrl));
        doReturn(null).when(mSearchUrlHelperJniMock).getQueryIfValidSrpUrl(eq(mNonSrpUrl));
        ContinuousNavigationUserDataImpl.setInstanceForTesting(mUserDataMock);

        SearchResultProducerFactory.overrideFactory(new FakeSearchResultProducerFactory());

        mObserver = new ContinuousSearchTabObserver(mTabMock);
        verify(mTabMock, times(1)).addObserver(eq(mObserver));
    }

    @After
    public void tearDown() {
        mObserver.onDestroyed(mTabMock);
        verify(mTabMock, times(1)).removeObserver(mObserver);
    }

    /**
     * Verifies that nothing significant occurs when a non-SRP URL is loaded.
     */
    @Test
    public void testLoadNonSrpUrl() {
        InOrder inOrder = inOrder(mUserDataMock);

        mObserver.onPageLoadStarted(mTabMock, mNonSrpUrl);
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mNonSrpUrl));

        mObserver.onPageLoadFinished(mTabMock, mNonSrpUrl);
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mNonSrpUrl));
    }

    /**
     * Verifies that results are requested and handled when a SRP URL is loaded.
     */
    @Test
    public void testLoadSrpUrl() {
        InOrder inOrder = inOrder(mUserDataMock, mProducerMock);

        mObserver.onPageLoadStarted(mTabMock, mSrpUrl);
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mSrpUrl));

        mObserver.onPageLoadFinished(mTabMock, mSrpUrl);
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mSrpUrl));
        inOrder.verify(mProducerMock).fetchResults(eq(mSrpUrl), eq(TEST_QUERY));

        ContinuousNavigationMetadata metadata = mock(ContinuousNavigationMetadata.class);
        doReturn(mSrpUrl).when(mTabMock).getUrl();
        mObserver.onResult(metadata);
        inOrder.verify(mUserDataMock).updateData(eq(metadata), eq(mSrpUrl));

        inOrder.verifyNoMoreInteractions();
    }

    /**
     * Verifies that results are requested when a SRP URL is loaded and errors are handled.
     */
    @Test
    public void testLoadSrpUrlWithError() {
        InOrder inOrder = inOrder(mUserDataMock, mProducerMock);

        mObserver.onPageLoadStarted(mTabMock, mSrpUrl);
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mSrpUrl));

        mObserver.onPageLoadFinished(mTabMock, mSrpUrl);
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mSrpUrl));
        inOrder.verify(mProducerMock).fetchResults(eq(mSrpUrl), eq(TEST_QUERY));

        mObserver.onError(1);
        inOrder.verifyNoMoreInteractions();
    }

    /**
     * Verifies that results are requested when a SRP URL is loaded and interruption with closing
     * contents is handled.
     */
    @Test
    public void testCloseContents() {
        InOrder inOrder = inOrder(mUserDataMock, mProducerMock);

        mObserver.onPageLoadStarted(mTabMock, mSrpUrl);
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mSrpUrl));

        mObserver.onPageLoadFinished(mTabMock, mSrpUrl);
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mSrpUrl));
        inOrder.verify(mProducerMock).fetchResults(eq(mSrpUrl), eq(TEST_QUERY));

        mObserver.onCloseContents(mTabMock);
        inOrder.verify(mUserDataMock).invalidateData();
        inOrder.verifyNoMoreInteractions();
    }

    /**
     * Verifies that changing activity attachment is a no-op.
     */
    @Test
    public void testOnActivityAttachmentChanged() {
        // Test no-op.
        mObserver.onActivityAttachmentChanged(mTabMock, null);
    }
}
