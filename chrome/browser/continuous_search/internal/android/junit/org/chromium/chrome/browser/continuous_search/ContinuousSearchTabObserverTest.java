// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.continuous_search.SearchResultExtractorClientStatus;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Tests for {@link ContinuousSearchTabObserver}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowRecordHistogram.class})
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
    @Mock
    private SearchResultExtractorProducer.Natives mSearchResultExtractorProducerJniMock;

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
        ShadowRecordHistogram.reset();

        mSrpUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
        mNonSrpUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);

        initMocks(this);
        mJniMocker.mock(SearchUrlHelperJni.TEST_HOOKS, mSearchUrlHelperJniMock);
        doReturn(TEST_QUERY).when(mSearchUrlHelperJniMock).getQueryIfValidSrpUrl(eq(mSrpUrl));
        doReturn(null).when(mSearchUrlHelperJniMock).getQueryIfValidSrpUrl(eq(mNonSrpUrl));
        doReturn(false).when(mUserDataMock).isMatchingSrp(eq(mSrpUrl));
        ContinuousNavigationUserDataImpl.setInstanceForTesting(mUserDataMock);

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

        mObserver.onUpdateUrl(mTabMock, mNonSrpUrl);
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

        mObserver.onUpdateUrl(mTabMock, mSrpUrl);
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
     * Verifies that results are requested when a SRP URL is loaded and errors are handled.
     */
    @Test
    public void testLoadSrpUrlWithError() {
        InOrder inOrder = inOrder(mUserDataMock, mProducerMock);

        mObserver.onUpdateUrl(mTabMock, mSrpUrl);
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

        mObserver.onUpdateUrl(mTabMock, mSrpUrl);
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

        mObserver.onUpdateUrl(mTabMock, mSrpUrl);
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

    /**
     * Verifies that histogram recording works.
     */
    @Test
    @EnableFeatures({ChromeFeatureList.CONTINUOUS_SEARCH})
    public void testHistogramRecording() {
        SearchResultProducerFactory.overrideFactory(null);
        mJniMocker.mock(
                SearchResultExtractorProducerJni.TEST_HOOKS, mSearchResultExtractorProducerJniMock);
        final long nativePtr = 123L;
        doReturn(nativePtr).when(mSearchResultExtractorProducerJniMock).create(any());

        mObserver.onUpdateUrl(mTabMock, mSrpUrl);
        mObserver.onPageLoadFinished(mTabMock, mSrpUrl);

        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Browser.ContinuousSearch.SearchResultExtractionStatus",
                        SearchResultExtractorClientStatus.WEB_CONTENTS_GONE));
    }
}
