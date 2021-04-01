// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static junit.framework.Assert.assertEquals;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Collection;

/**
 * Unit tests for the {@link BackNavigationTabObserver} class.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(shadows = {ShadowRecordHistogram.class})
public class BackNavigationTabObserverTest {
    private static final String HISTOGRAM = "Browser.ContinuousSearch.BackNavigationToSrp";

    @Rule
    public JniMocker mMocker = new JniMocker();

    @Mock
    private Tab mTab;
    @Mock
    private NavigationController mNavigationController;
    @Mock
    private SearchUrlHelper.Natives mSearchUrlHelperJniMock;

    private BackNavigationTabObserver mBackNavigationTabObserver;
    private final @PageCategory int mPageCategory;
    private final String mHistogramSuffix;

    public BackNavigationTabObserverTest(@PageCategory int pageCategory, String histogramSuffix) {
        mPageCategory = pageCategory;
        mHistogramSuffix = histogramSuffix;
    }

    @ParameterizedRobolectricTestRunner.Parameters
    public static Collection resultCategories() {
        return Arrays.asList(new Object[][] {
                {PageCategory.ORGANIC_SRP, ".Organic"}, {PageCategory.NEWS_SRP, ".News"}});
    }

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        mMocker.mock(SearchUrlHelperJni.TEST_HOOKS, mSearchUrlHelperJniMock);
        mBackNavigationTabObserver = new BackNavigationTabObserver(mTab);

        WebContents webContents = mock(WebContents.class);
        when(mTab.getWebContents()).thenReturn(webContents);
        when(webContents.getNavigationController()).thenReturn(mNavigationController);

        GURL searchUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL);
        GURL searchUrl2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_2_URL);
        doReturn("test").when(mSearchUrlHelperJniMock).getQueryIfValidSrpUrl(eq(searchUrl));
        doReturn("query").when(mSearchUrlHelperJniMock).getQueryIfValidSrpUrl(eq(searchUrl2));
        doReturn(null)
                .when(mSearchUrlHelperJniMock)
                .getQueryIfValidSrpUrl(eq(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL)));
        doReturn(null)
                .when(mSearchUrlHelperJniMock)
                .getQueryIfValidSrpUrl(eq(JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1)));
        doReturn(null)
                .when(mSearchUrlHelperJniMock)
                .getQueryIfValidSrpUrl(eq(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1)));
        doReturn(true).when(mSearchUrlHelperJniMock).isGoogleDomainUrl(eq(searchUrl));
        doReturn(false).when(mSearchUrlHelperJniMock).isGoogleDomainUrl(eq(GURL.emptyGURL()));
        doReturn(mPageCategory)
                .when(mSearchUrlHelperJniMock)
                .getSrpPageCategoryFromUrl(eq(searchUrl));
        doReturn(mPageCategory)
                .when(mSearchUrlHelperJniMock)
                .getSrpPageCategoryFromUrl(eq(searchUrl2));
    }

    private NavigationEntry createNavigationEntry(GURL url) {
        return new NavigationEntry(
                0, url, GURL.emptyGURL(), GURL.emptyGURL(), GURL.emptyGURL(), "", null, 0, 0);
    }

    private NavigationEntry createNavigationEntry(GURL url, GURL referrer) {
        return new NavigationEntry(
                0, url, GURL.emptyGURL(), GURL.emptyGURL(), referrer, "", null, 0, 0);
    }

    private void setNavigationHistory(NavigationEntry entry) {
        NavigationHistory history = new NavigationHistory();
        history.addEntry(entry);
        history.setCurrentEntryIndex(0);

        when(mNavigationController.getNavigationHistory()).thenReturn(history);
    }

    private void navigateThroughEntries(NavigationEntry... entries) {
        for (NavigationEntry entry : entries) {
            setNavigationHistory(entry);
            mBackNavigationTabObserver.onPageLoadFinished(mTab, entry.getUrl());
        }
    }

    private void assertHistogramRecorded(int expectedCount, int bucket) {
        assertEquals(expectedCount,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM + mHistogramSuffix, bucket));
    }

    @Test
    public void testEndSessionWithAnotherSrp() {
        navigateThroughEntries(
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL),
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1),
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_2_URL)));

        assertHistogramRecorded(1, 2);
    }

    @Test
    public void testEndSessionWithAnotherSrp_reload() {
        navigateThroughEntries(
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL),
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_2_URL)));

        assertHistogramRecorded(1, 1);
    }

    @Test
    public void testEndSessionWithSameSrpDifferentCategory() {
        navigateThroughEntries(
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL),
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1),
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)));
        when(mSearchUrlHelperJniMock.getSrpPageCategoryFromUrl(
                     eq(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL))))
                .thenReturn(PageCategory.NONE);
        navigateThroughEntries(
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)));

        assertHistogramRecorded(1, 1);
    }

    @Test
    public void testEndSessionWithTabDestroyed() {
        navigateThroughEntries(
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL),
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1),
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)));
        mBackNavigationTabObserver.onDestroyed(mTab);

        assertHistogramRecorded(1, 1);
        verify(mTab, times(1)).removeObserver(eq(mBackNavigationTabObserver));
    }

    @Test
    public void testEndSessionOnContentChanged() {
        navigateThroughEntries(
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL),
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1),
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)));

        when(mTab.isNativePage()).thenReturn(true);
        mBackNavigationTabObserver.onContentChanged(mTab);
        assertHistogramRecorded(1, 1);
    }

    @Test
    public void testEndSessionOnHidden() {
        navigateThroughEntries(
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL),
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1),
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)));

        mBackNavigationTabObserver.onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);
        assertHistogramRecorded(1, 1);
    }

    @Test
    public void testRecordSessionWithNoBackNavigation() {
        navigateThroughEntries(
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL),
                        JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_2_URL)));

        assertHistogramRecorded(1, 0);
    }

    @Test
    public void testNotRecordWhenNotSeenSrp() {
        navigateThroughEntries(
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)));

        assertHistogramRecorded(0, 0);
    }

    @Test
    public void testNotRecordWhenSrpAbandoned() {
        navigateThroughEntries(
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL)),
                createNavigationEntry(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_2_URL)));

        assertHistogramRecorded(0, 0);
    }
}
