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
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
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
    private WebContents mWebContentsMock;
    @Mock
    private SearchUrlHelper.Natives mSearchUrlHelperJniMock;
    @Mock
    private ContinuousNavigationUserDataImpl mUserDataMock;
    @Mock
    private SearchResultProducer mProducerMock;
    @Mock
    private SearchResultExtractorProducer.Natives mSearchResultExtractorProducerJniMock;
    @Mock
    private NavigationHandle mNavigationHandleMock;

    private boolean mNeedsTeardown;
    private GURL mSrpUrl;
    private GURL mNonSrpUrl;
    private ContinuousSearchTabObserver mObserver;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
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
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CONTINUOUS_SEARCH, true);
        FeatureList.setTestValues(testValues);

        mSrpUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
        mNonSrpUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);

        initMocks(this);
        mJniMocker.mock(SearchUrlHelperJni.TEST_HOOKS, mSearchUrlHelperJniMock);
        doReturn(TEST_QUERY).when(mSearchUrlHelperJniMock).getQueryIfValidSrpUrl(eq(mSrpUrl));
        doReturn(null).when(mSearchUrlHelperJniMock).getQueryIfValidSrpUrl(eq(mNonSrpUrl));
        doReturn(false).when(mUserDataMock).isMatchingSrp(eq(mSrpUrl));
        doReturn(true).when(mUserDataMock).isValid();
        ContinuousNavigationUserDataImpl.setInstanceForTesting(mUserDataMock);
        doReturn(true).when(mNavigationHandleMock).hasCommitted();
        doReturn(true).when(mNavigationHandleMock).isInPrimaryMainFrame();
        doReturn(false).when(mNavigationHandleMock).isSameDocument();

        SearchResultProducerFactory.overrideFactory(new FakeSearchResultProducerFactory());

        mObserver = new ContinuousSearchTabObserver(mTabMock);
        verify(mTabMock, times(1)).addObserver(eq(mObserver));
        mNeedsTeardown = true;
    }

    @After
    public void tearDown() {
        if (mNeedsTeardown) {
            mObserver.onDestroyed(mTabMock);
            verify(mTabMock, times(1)).removeObserver(mObserver);
        }
    }

    /**
     * Verifies that nothing significant occurs when a non-SRP URL is loaded.
     */
    @Test
    public void testLoadNonSrpUrl() {
        InOrder inOrder = inOrder(mUserDataMock);

        doReturn(mNonSrpUrl).when(mNavigationHandleMock).getUrl();
        mObserver.onDidFinishNavigation(mTabMock, mNavigationHandleMock);
        inOrder.verify(mUserDataMock).isValid();
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mNonSrpUrl));

        doReturn(false).when(mUserDataMock).isMatchingSrp(eq(mNonSrpUrl));
        mObserver.onPageLoadFinished(mTabMock, mNonSrpUrl);
        inOrder.verify(mUserDataMock).isMatchingSrp(eq(mNonSrpUrl));
        inOrder.verifyNoMoreInteractions();
    }

    /**
     * Verifies that results are requested and handled when a SRP URL is loaded.
     */
    @Test
    public void testLoadSrpUrl() {
        InOrder inOrder = inOrder(mUserDataMock, mProducerMock);

        doReturn(mSrpUrl).when(mNavigationHandleMock).getUrl();
        mObserver.onDidFinishNavigation(mTabMock, mNavigationHandleMock);
        inOrder.verify(mUserDataMock).isValid();
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mSrpUrl));

        mObserver.onPageLoadFinished(mTabMock, mSrpUrl);
        inOrder.verify(mProducerMock).fetchResults(eq(mSrpUrl), eq(TEST_QUERY));

        ContinuousNavigationMetadata metadata = mock(ContinuousNavigationMetadata.class);
        doReturn(mSrpUrl).when(mTabMock).getUrl();
        mObserver.onResult(metadata);
        inOrder.verify(mUserDataMock).updateData(eq(metadata), eq(mSrpUrl));

        doReturn(true).when(mUserDataMock).isMatchingSrp(eq(mSrpUrl));
        mObserver.onPageLoadFinished(mTabMock, mSrpUrl);
        inOrder.verify(mUserDataMock).isMatchingSrp(eq(mSrpUrl));
        inOrder.verifyNoMoreInteractions();
    }

    /**
     * Verifies that update url handles redirects.
     */
    @Test
    public void testLoadRedirect() {
        InOrder inOrder = inOrder(mUserDataMock);
        doReturn(mWebContentsMock).when(mTabMock).getWebContents();

        // Case 1: empty/invalid GURL.
        doReturn(mSrpUrl).when(mNavigationHandleMock).getUrl();
        doReturn(GURL.emptyGURL())
                .when(mSearchUrlHelperJniMock)
                .getOriginalUrlFromWebContents(eq(mWebContentsMock));
        mObserver.onDidFinishNavigation(mTabMock, mNavigationHandleMock);
        inOrder.verify(mUserDataMock).isValid();
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mSrpUrl));

        // Case 2: different unmatched original GURL.
        mObserver.mOriginMatchesForTesting = new Boolean(false);
        GURL originalUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);
        GURL committedUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_2);
        doReturn(committedUrl).when(mNavigationHandleMock).getUrl();
        doReturn(originalUrl)
                .when(mSearchUrlHelperJniMock)
                .getOriginalUrlFromWebContents(eq(mWebContentsMock));
        mObserver.onDidFinishNavigation(mTabMock, mNavigationHandleMock);
        inOrder.verify(mUserDataMock).isValid();
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(committedUrl));

        // Case 3: different matched original GURL.
        mObserver.mOriginMatchesForTesting = new Boolean(true);
        doReturn(committedUrl).when(mNavigationHandleMock).getUrl();
        doReturn(originalUrl)
                .when(mSearchUrlHelperJniMock)
                .getOriginalUrlFromWebContents(eq(mWebContentsMock));
        mObserver.onDidFinishNavigation(mTabMock, mNavigationHandleMock);
        inOrder.verify(mUserDataMock).isValid();
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(originalUrl));

        // Check guards on {@link NavigationHandle} state.
        doReturn(true).when(mNavigationHandleMock).isSameDocument();
        mObserver.onDidFinishNavigation(mTabMock, mNavigationHandleMock);
        doReturn(false).when(mNavigationHandleMock).isSameDocument();

        doReturn(false).when(mNavigationHandleMock).isInPrimaryMainFrame();
        mObserver.onDidFinishNavigation(mTabMock, mNavigationHandleMock);
        doReturn(true).when(mNavigationHandleMock).isInPrimaryMainFrame();

        doReturn(false).when(mNavigationHandleMock).hasCommitted();
        mObserver.onDidFinishNavigation(mTabMock, mNavigationHandleMock);
        doReturn(true).when(mNavigationHandleMock).hasCommitted();

        doReturn(false).when(mUserDataMock).isValid();
        mObserver.onDidFinishNavigation(mTabMock, mNavigationHandleMock);
        inOrder.verify(mUserDataMock).isValid();

        inOrder.verifyNoMoreInteractions();
    }

    /**
     * Verifies that results are requested when a SRP URL is loaded and errors are handled.
     */
    @Test
    public void testLoadSrpUrlWithError() {
        InOrder inOrder = inOrder(mUserDataMock, mProducerMock);

        doReturn(mSrpUrl).when(mNavigationHandleMock).getUrl();
        mObserver.onDidFinishNavigation(mTabMock, mNavigationHandleMock);
        inOrder.verify(mUserDataMock).isValid();
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mSrpUrl));

        mObserver.onPageLoadFinished(mTabMock, mSrpUrl);
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

        doReturn(mSrpUrl).when(mNavigationHandleMock).getUrl();
        mObserver.onDidFinishNavigation(mTabMock, mNavigationHandleMock);
        inOrder.verify(mUserDataMock).isValid();
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mSrpUrl));

        mObserver.onPageLoadFinished(mTabMock, mSrpUrl);
        inOrder.verify(mProducerMock).fetchResults(eq(mSrpUrl), eq(TEST_QUERY));

        mObserver.onCloseContents(mTabMock);
        inOrder.verify(mUserDataMock).invalidateData();
        inOrder.verifyNoMoreInteractions();
    }

    /**
     * Verifies that results are requested and handled when a SRP URL is loaded.
     */
    @Test
    public void testLoadSrpUrlThenCloseTab() {
        InOrder inOrder = inOrder(mUserDataMock, mProducerMock, mTabMock);

        doReturn(mSrpUrl).when(mNavigationHandleMock).getUrl();
        mObserver.onDidFinishNavigation(mTabMock, mNavigationHandleMock);
        inOrder.verify(mUserDataMock).isValid();
        inOrder.verify(mUserDataMock).updateCurrentUrl(eq(mSrpUrl));

        mObserver.onPageLoadFinished(mTabMock, mSrpUrl);
        inOrder.verify(mProducerMock).fetchResults(eq(mSrpUrl), eq(TEST_QUERY));

        // Close the tab.
        mNeedsTeardown = false;
        mObserver.onDestroyed(mTabMock);
        ContinuousNavigationUserDataImpl.setInstanceForTesting(null);

        // Metadata will not be returned as the destruction of the tab will cancel the request.
        inOrder.verify(mProducerMock).cancel();
        inOrder.verify(mUserDataMock).invalidateData();
        inOrder.verify(mTabMock).removeObserver(mObserver);

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
