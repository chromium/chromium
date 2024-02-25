// Copyright 2017 The Chromium Authors
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
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.DocumentMetadata;
import org.chromium.blink.mojom.WebPage;
import org.chromium.chrome.browser.historyreport.AppIndexingReporter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.mojom.Url;

/** Unit tests for {@link org.chromium.chrome.browser.AppIndexingUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AppIndexingUtilTest {
    @Spy private AppIndexingUtil mUtil = new AppIndexingUtil(null);
    @Mock private AppIndexingReporter mReporter;
    @Mock private DocumentMetadataTestImpl mDocumentMetadata;
    @Mock private Tab mTab;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mReporter).when(mUtil).getAppIndexingReporter();
        doReturn(mDocumentMetadata).when(mUtil).getDocumentMetadataInterface(any(Tab.class));
        doReturn(true).when(mUtil).isEnabledForDevice();
        doReturn(false).when(mTab).isIncognito();

        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mTab).getUrl();
        doReturn("My neat website").when(mTab).getTitle();
        doReturn(0L).when(mUtil).getElapsedTime();
        doAnswer(
                        invocation -> {
                            DocumentMetadata.GetEntities_Response callback =
                                    (DocumentMetadata.GetEntities_Response)
                                            invocation.getArguments()[0];
                            WebPage webpage = new WebPage();
                            webpage.url = createUrl(JUnitTestGURLs.EXAMPLE_URL.getSpec());
                            webpage.title = "My neat website";
                            callback.call(webpage);
                            return null;
                        })
                .when(mDocumentMetadata)
                .getEntities(any(DocumentMetadata.GetEntities_Response.class));
    }

    @Test
    public void testExtractDocumentMetadata_Incognito() {
        doReturn(true).when(mTab).isIncognito();

        mUtil.extractDocumentMetadata(mTab);
        verify(mDocumentMetadata, never()).getEntities(any());
        verify(mReporter, never()).reportWebPage(any());
    }

    @Test
    public void testExtractDocumentMetadata_NoCacheHit() {
        mUtil.extractDocumentMetadata(mTab);
        verify(mDocumentMetadata).getEntities(any(DocumentMetadata.GetEntities_Response.class));
        verify(mReporter).reportWebPage(any(WebPage.class));
    }

    @Test
    public void testExtractDocumentMetadata_CacheHit() {
        mUtil.extractDocumentMetadata(mTab);
        verify(mDocumentMetadata).getEntities(any(DocumentMetadata.GetEntities_Response.class));
        verify(mDocumentMetadata).close();
        verify(mReporter).reportWebPage(any(WebPage.class));

        doReturn(1L).when(mUtil).getElapsedTime();
        mUtil.extractDocumentMetadata(mTab);
        verifyNoMoreInteractions(mDocumentMetadata);
        verifyNoMoreInteractions(mReporter);
    }

    @Test
    public void testExtractDocumentMetadata_CacheHit_Expired() {
        mUtil.extractDocumentMetadata(mTab);

        doReturn(60 * 60 * 1000L + 1).when(mUtil).getElapsedTime();
        mUtil.extractDocumentMetadata(mTab);
        verify(mDocumentMetadata, times(2))
                .getEntities(any(DocumentMetadata.GetEntities_Response.class));
    }

    @Test
    public void testExtractDocumentMetadata_CacheHit_NoEntity() {
        doAnswer(
                        invocation -> {
                            DocumentMetadata.GetEntities_Response callback =
                                    (DocumentMetadata.GetEntities_Response)
                                            invocation.getArguments()[0];
                            callback.call(null);
                            return null;
                        })
                .when(mDocumentMetadata)
                .getEntities(any(DocumentMetadata.GetEntities_Response.class));
        mUtil.extractDocumentMetadata(mTab);

        doReturn(1L).when(mUtil).getElapsedTime();
        mUtil.extractDocumentMetadata(mTab);
        verify(mDocumentMetadata, times(1))
                .getEntities(any(DocumentMetadata.GetEntities_Response.class));
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
        verify(mReporter)
                .reportWebPageView(eq(JUnitTestGURLs.EXAMPLE_URL.getSpec()), eq("My neat website"));
    }

    private Url createUrl(String s) {
        Url url = new Url();
        url.url = s;
        return url;
    }

    abstract static class DocumentMetadataTestImpl implements DocumentMetadata {}
}
