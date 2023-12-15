// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.text.TextUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.url.JUnitTestGURLs;

/** Tests {@link ShareSheetLinkToggleCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ShareSheetLinkToggleCoordinatorTest {
    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;
    @Mock private LinkToTextCoordinator mLinkToTextCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        jniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);
        when(mDistillerUrlUtilsJniMock.getOriginalUrlFromDistillerUrl(any(String.class)))
                .thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        ShareParams shareParamsWithLinkToText =
                new ShareParams.Builder(
                                /* window= */ null,
                                /* title= */ "",
                                JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .setText("text")
                        .build();
        when(mLinkToTextCoordinator.getShareParams(LinkToggleState.LINK))
                .thenReturn(shareParamsWithLinkToText);
        ShareParams shareParamsWithTextOnly =
                new ShareParams.Builder(/* window= */ null, /* title= */ "", /* url= */ "")
                        .setText("text")
                        .build();
        when(mLinkToTextCoordinator.getShareParams(LinkToggleState.NO_LINK))
                .thenReturn(shareParamsWithTextOnly);
    }

    @Test
    public void getShareParams_urlShare_sameShareParams() {
        ShareParams shareParams =
                new ShareParams.Builder(
                                /* window= */ null,
                                /* title= */ "",
                                JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(
                        shareParams, chromeShareExtras, /* linkToTextCoordinator= */ null);

        assertEquals(
                "ShareParams should be the same as the original.",
                shareParams,
                shareSheetLinkToggleCoordinator.getShareParams(LinkToggleState.NO_LINK));
    }

    @Test
    public void getShareParams_nonUrlShare_noLink() {
        ShareParams shareParams =
                new ShareParams.Builder(/* window= */ null, /* title= */ "", "").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .setContentUrl(JUnitTestGURLs.EXAMPLE_URL)
                        .build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(
                        shareParams, chromeShareExtras, /* linkToTextCoordinator= */ null);

        assertTrue(
                "ShareParams should not have a URL.",
                TextUtils.isEmpty(
                        shareSheetLinkToggleCoordinator
                                .getShareParams(LinkToggleState.NO_LINK)
                                .getUrl()));
    }

    @Test
    public void getShareParams_nonUrlShare_link() {
        ShareParams shareParams =
                new ShareParams.Builder(/* window= */ null, /* title= */ "", "").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setContentUrl(JUnitTestGURLs.EXAMPLE_URL)
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(
                        shareParams, chromeShareExtras, /* linkToTextCoordinator= */ null);

        assertEquals(
                "ShareParams should include the link.",
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                shareSheetLinkToggleCoordinator.getShareParams(LinkToggleState.LINK).getUrl());
    }

    @Test
    public void getShareParams_linkToText_noLink() {
        ShareParams shareParams =
                new ShareParams.Builder(/* window= */ null, /* title= */ "", "")
                        .setText("text")
                        .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(
                        shareParams, chromeShareExtras, mLinkToTextCoordinator);

        assertTrue(
                "ShareParams should not have a URL.",
                TextUtils.isEmpty(
                        shareSheetLinkToggleCoordinator
                                .getShareParams(LinkToggleState.NO_LINK)
                                .getUrl()));
    }

    @Test
    public void getShareParams_linkToText_link() {
        ShareParams shareParams =
                new ShareParams.Builder(/* window= */ null, /* title= */ "", "")
                        .setText("text")
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.HIGHLIGHTED_TEXT)
                        .build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(
                        shareParams, chromeShareExtras, mLinkToTextCoordinator);

        assertEquals(
                "ShareParams should include the link.",
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                shareSheetLinkToggleCoordinator.getShareParams(LinkToggleState.LINK).getUrl());
    }

    @Test
    public void shouldShowToggle_noDetailedContentType_returnsFalse() {
        ShareParams shareParams =
                new ShareParams.Builder(
                                /* window= */ null,
                                /* title= */ "",
                                JUnitTestGURLs.EXAMPLE_URL.getSpec())
                        .build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setContentUrl(JUnitTestGURLs.EXAMPLE_URL).build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(
                        shareParams, chromeShareExtras, /* linkToTextCoordinator= */ null);

        assertFalse("Should not show toggle.", shareSheetLinkToggleCoordinator.shouldShowToggle());
    }

    @Test
    public void shouldShowToggle_noContentUrl_returnsFalse() {
        ShareParams shareParams =
                new ShareParams.Builder(/* window= */ null, /* title= */ "", /* url= */ "").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(
                        shareParams, chromeShareExtras, /* linkToTextCoordinator= */ null);

        assertFalse("Should not show toggle.", shareSheetLinkToggleCoordinator.shouldShowToggle());
    }

    @Test
    public void shouldShowToggle_contentUrlAndDetailedContentType_returnsTrue() {
        ShareParams shareParams =
                new ShareParams.Builder(/* window= */ null, /* title= */ "", /* url= */ "").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .setContentUrl(JUnitTestGURLs.EXAMPLE_URL)
                        .build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(
                        shareParams, chromeShareExtras, /* linkToTextCoordinator= */ null);

        assertTrue("Should show toggle.", shareSheetLinkToggleCoordinator.shouldShowToggle());
    }

    @Test
    public void shouldShowToggle_linkToText_returnsTrue() {
        ShareParams shareParams =
                new ShareParams.Builder(/* window= */ null, /* title= */ "", /* url= */ "").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.HIGHLIGHTED_TEXT)
                        .build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(
                        shareParams, chromeShareExtras, mLinkToTextCoordinator);

        assertTrue("Should show toggle.", shareSheetLinkToggleCoordinator.shouldShowToggle());
    }

    @Test
    public void shouldEnableToggleByDefault_false() {
        ShareParams shareParams =
                new ShareParams.Builder(/* window= */ null, /* title= */ "", /* url= */ "").build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(
                        shareParams, chromeShareExtras, mLinkToTextCoordinator);

        assertFalse(
                "Should not enable toggle by default.",
                shareSheetLinkToggleCoordinator.shouldEnableToggleByDefault());

        shareSheetLinkToggleCoordinator.setShareParamsAndExtras(
                shareParams,
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.GIF)
                        .build());
        assertFalse(
                "Should not enable toggle by default.",
                shareSheetLinkToggleCoordinator.shouldEnableToggleByDefault());

        shareSheetLinkToggleCoordinator.setShareParamsAndExtras(
                shareParams,
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.SCREENSHOT)
                        .build());
        assertFalse(
                "Should not enable toggle by default.",
                shareSheetLinkToggleCoordinator.shouldEnableToggleByDefault());

        shareSheetLinkToggleCoordinator.setShareParamsAndExtras(
                shareParams,
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.IMAGE)
                        .build());
        assertFalse(
                "Should not enable toggle by default.",
                shareSheetLinkToggleCoordinator.shouldEnableToggleByDefault());
    }

    @Test
    public void shouldEnableToggleByDefault_true() {
        ShareParams shareParams =
                new ShareParams.Builder(/* window= */ null, /* title= */ "", /* url= */ "").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.HIGHLIGHTED_TEXT)
                        .build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(
                        shareParams, chromeShareExtras, mLinkToTextCoordinator);

        assertTrue(
                "Should enable toggle by default.",
                shareSheetLinkToggleCoordinator.shouldEnableToggleByDefault());
    }
}
