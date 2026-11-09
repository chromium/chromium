// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.net.Uri;
import android.os.Build;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FileProviderUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestOrigins;
import org.chromium.url.Origin;

/**
 * Unit tests for {@link DownloadController}.
 *
 * <p>Covers inline PDF initiator-origin propagation (crbug.com/500173014) and {@link
 * DownloadController#downloadLocalFile} {@code file://} normalization.
 *
 * <p>VANILLA_ICE_CREAM satisfies {@code PdfUtils.isPlatformSupported()}, which gates inline PDF
 * viewing.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
public class DownloadControllerUnitTest {
    private static final String TAB_URL = "https://initiator.example/landing";
    private static final String PDF_URL = "https://victim.example/document.pdf";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private DownloadController.Natives mDownloadControllerJniMock;

    @Before
    public void setUp() {
        DownloadControllerJni.setInstanceForTesting(mDownloadControllerJniMock);
        when(mTab.getWebContents()).thenReturn(mWebContents);
    }

    @After
    public void tearDown() {
        FileProviderUtils.setFileProviderUtil(null);
    }

    private LoadUrlParams captureLoadUrlParams(@Nullable Origin initiatorOrigin) {
        when(mTab.getUrl()).thenReturn(new GURL(TAB_URL));
        when(mTab.isIncognito()).thenReturn(false);

        DownloadInfo info = new DownloadInfo.Builder().setUrl(new GURL(PDF_URL)).build();
        DownloadController.onPdfDownloadStarted(mTab, info, initiatorOrigin);

        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mTab).loadUrl(captor.capture());
        return captor.getValue();
    }

    /**
     * A download initiated by a page must record its initiator on the synthesized entry. Without
     * this, a later traversal re-downloads with no initiator, which
     * net::cookie_util::ComputeSameSiteContext reads as browser-initiated and grants a
     * SAME_SITE_STRICT context - the cross-site cookie bypass in crbug.com/500173014.
     */
    @Test
    @Feature({"Download"})
    public void testOnPdfDownloadStarted_PropagatesInitiatorOrigin() {
        Origin initiatorOrigin = JUnitTestOrigins.createTuple("https", "initiator.example", 443);

        LoadUrlParams param = captureLoadUrlParams(initiatorOrigin);

        assertEquals(initiatorOrigin, param.getInitiatorOrigin());
        assertTrue(param.getIsPdf());
        assertEquals(PDF_URL, param.getVirtualUrlForSpecialCases());
        // The initiator origin is what fixes the cookie bug; the navigation itself stays
        // browser-initiated, since nothing here knows whether the triggering navigation was
        // renderer-initiated.
        assertFalse(param.getIsRendererInitiated());
    }

    /**
     * A download with no initiator is genuinely browser-initiated, e.g. a PDF opened from the
     * omnibox, a bookmark or an external intent. It must stay that way: fabricating an initiator
     * here would strip SameSite=Strict cookies from the later re-download and break authenticated
     * PDFs.
     */
    @Test
    @Feature({"Download"})
    public void testOnPdfDownloadStarted_NullInitiatorOriginStaysNull() {
        LoadUrlParams param = captureLoadUrlParams(/* initiatorOrigin= */ null);

        assertNull(param.getInitiatorOrigin());
        assertFalse(param.getIsRendererInitiated());
        assertTrue(param.getIsPdf());
    }

    @Test
    @Feature({"Download"})
    public void testOnDownloadFailed_NonPdf_NoOp() {
        when(mTab.isIncognito()).thenReturn(false);
        DownloadInfo info =
                new DownloadInfo.Builder()
                        .setUrl(new GURL(PDF_URL))
                        .setMimeType("text/plain")
                        .setIsTransient(true)
                        .build();

        DownloadController.onDownloadFailed(mTab, info);

        verify(mTab, never()).getNativePage();
    }

    @Test
    @Feature({"Download"})
    public void testOnDownloadFailed_InlinePdf_NotifiesPdfPage() {
        PdfPage pdfPage = mock(PdfPage.class);
        when(pdfPage.isPdf()).thenReturn(true);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(pdfPage);
        DownloadInfo info =
                new DownloadInfo.Builder()
                        .setUrl(new GURL(PDF_URL))
                        .setMimeType(MimeTypeUtils.PDF_MIME_TYPE)
                        .setIsTransient(true)
                        .build();

        DownloadController.onDownloadFailed(mTab, info);

        verify(pdfPage).onDownloadFailed();
    }

    @Test
    @Feature({"Download"})
    public void testDownloadLocalFile_UncoveredFileUri_PreservesFileUri() {
        FileProviderUtils.setFileProviderUtil(
                file -> {
                    throw new IllegalArgumentException("Uncovered path: " + file);
                });

        DownloadController.downloadLocalFile("file:///tmp/sample.pdf", "sample.pdf", mTab);

        verify(mDownloadControllerJniMock)
                .downloadLocalFile("file:///tmp/sample.pdf", "sample.pdf", mWebContents);
    }

    @Test
    @Feature({"Download"})
    public void testDownloadLocalFile_CoveredFileUri_RewritesToContentUri() {
        String providerUri = "content://com.android.chrome.FileProvider/downloads/sample.pdf";
        FileProviderUtils.setFileProviderUtil(file -> Uri.parse(providerUri));

        DownloadController.downloadLocalFile(
                "file:///storage/emulated/0/Download/sample.pdf", "annotated.pdf", mTab);

        verify(mDownloadControllerJniMock)
                .downloadLocalFile(providerUri, "annotated.pdf", mWebContents);
    }

    @Test
    @Feature({"Download"})
    public void testDownloadLocalFile_ContentUri_PassedThroughUnchanged() {
        String contentUri = "content://media/external/downloads/42";

        DownloadController.downloadLocalFile(contentUri, /* suggestedFileName= */ null, mTab);

        verify(mDownloadControllerJniMock).downloadLocalFile(contentUri, "", mWebContents);
    }

    @Test
    @Feature({"Download"})
    public void testDownloadLocalFile_CoveredFileUriWithNullSuggestedFileName_ForwardsBasename() {
        String opaqueProviderUri = "content://com.android.chrome.FileProvider/BlockedFile_1";
        FileProviderUtils.setFileProviderUtil(file -> Uri.parse(opaqueProviderUri));

        DownloadController.downloadLocalFile(
                "file:///storage/emulated/0/Download/sample.pdf",
                /* suggestedFileName= */ null,
                mTab);

        verify(mDownloadControllerJniMock)
                .downloadLocalFile(opaqueProviderUri, "sample.pdf", mWebContents);
    }
}
