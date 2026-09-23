// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Build;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestOrigins;
import org.chromium.url.Origin;

/**
 * Unit tests for {@link DownloadController}'s inline PDF handling.
 *
 * <p>These cover the initiator origin recorded on the synthesized {@code chrome-native://pdf}
 * entry. That entry is replayed on every later history traversal or tab restore, so whatever is
 * stored here determines the SameSite context of the eventual re-download. See crbug.com/500173014.
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
}
