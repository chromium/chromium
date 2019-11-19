// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.test.DisableHistogramsRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.document_metadata.CopylessPaste;
import org.chromium.blink.mojom.document_metadata.WebPage;
import org.chromium.chrome.browser.historyreport.AppIndexingReporter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.mojom.Url;

/**
 * Unit tests for {@link org.chromium.chrome.browser.AppIndexingUtil}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AppIndexingUtilTest {
    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();
    @Spy
    private AppIndexingUtil mUtil = new AppIndexingUtil(null);
    @Mock
    private AppIndexingReporter mReporter;
    @Mock
    private CopylessPasteTestImpl mCopylessPaste;
    @Mock
    private Tab mTab;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mReporter).when(mUtil).getAppIndexingReporter();
        doReturn(mCopylessPaste).when(mUtil).getCopylessPasteInterface(any(Tab.class));
        doReturn(true).when(mUtil).isEnabledForDevice();
        doReturn(false).when(mTab).isIncognito();

        doReturn("http://www.test.com").when(mTab).getUrl();
        doReturn("My neat website").when(mTab).getTitle();
        doReturn(0L).when(mUtil).getElapsedTime();
        doAnswer(invocation -> {
            CopylessPaste.GetEntitiesResponse callback =
                    (CopylessPaste.GetEntitiesResponse) invocation.getArguments()[0];
            WebPage webpage = new WebPage();
            webpage.url = createUrl("http://www.test.com");
            webpage.title = "My neat website";
            callback.call(webpage);
            return null;
        }).when(mCopylessPaste).getEntities(any(CopylessPaste.GetEntitiesResponse.class));
    }

    @Test
    public void testExtractCopylessPasteMetadata_Incognito() {
        doReturn(true).when(mTab).isIncognito();

        mUtil.extractCopylessPasteMetadata(mTab);
        verify(mCopylessPaste, never()).getEntities(any());
        verify(mReporter, never()).reportWebPage(any());
    }

    @Test
    public void testExtractCopylessPasteMetadata_NoCacheHit() {
        mUtil.extractCopylessPasteMetadata(mTab);
        verify(mCopylessPaste).getEntities(any(CopylessPaste.GetEntitiesResponse.class));
        verify(mReporter).reportWebPage(any(WebPage.class));
    }

    @Test
    public void testExtractCopylessPasteMetadata_CacheHit() {
        mUtil.extractCopylessPasteMetadata(mTab);
        verify(mCopylessPaste).getEntities(any(CopylessPaste.GetEntitiesResponse.class));
        verify(mCopylessPaste).close();
        verify(mReporter).reportWebPage(any(WebPage.class));

        doReturn(1L).when(mUtil).getElapsedTime();
        mUtil.extractCopylessPasteMetadata(mTab);
        verifyNoMoreInteractions(mCopylessPaste);
        verifyNoMoreInteractions(mReporter);
    }

    @Test
    public void testExtractCopylessPasteMetadata_CacheHit_Expired() {
        mUtil.extractCopylessPasteMetadata(mTab);

        doReturn(60 * 60 * 1000L + 1).when(mUtil).getElapsedTime();
        mUtil.extractCopylessPasteMetadata(mTab);
        verify(mCopylessPaste, times(2)).getEntities(any(CopylessPaste.GetEntitiesResponse.class));
    }

    @Test
    public void testExtractCopylessPasteMetadata_CacheHit_NoEntity() {
        doAnswer(invocation -> {
            CopylessPaste.GetEntitiesResponse callback =
                    (CopylessPaste.GetEntitiesResponse) invocation.getArguments()[0];
            callback.call(null);
            return null;
        }).when(mCopylessPaste).getEntities(any(CopylessPaste.GetEntitiesResponse.class));
        mUtil.extractCopylessPasteMetadata(mTab);

        doReturn(1L).when(mUtil).getElapsedTime();
        mUtil.extractCopylessPasteMetadata(mTab);
        verify(mCopylessPaste, times(1)).getEntities(any(CopylessPaste.GetEntitiesResponse.class));
        verifyNoMoreInteractions(mReporter);
    }

    @Test
    public void testReportPageView_Incognito() {
        doReturn(true).when(mTab).isIncognito();

        mUtil.reportPageView(mTab);
        verify(mReporter, never()).reportWebPageView(any(), any());
    }

    @Test
    public void testReportPageView() {
        mUtil.reportPageView(mTab);
        verify(mReporter).reportWebPageView(eq("http://www.test.com"), eq("My neat website"));
    }

    private Url createUrl(String s) {
        Url url = new Url();
        url.url = s;
        return url;
    }

    abstract static class CopylessPasteTestImpl implements CopylessPaste {}
}
