// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.text.TextUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.url.JUnitTestGURLs;

/**
 * Tests {@link ShareSheetLinkToggleCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION,
        ChromeFeatureList.SHARING_HUB_LINK_TOGGLE})
public class ShareSheetLinkToggleCoordinatorTest {
    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Mock
    private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;
    @Mock
    private ChromeOptionShareCallback mShareCallback;
    @Mock
    private LinkToTextCoordinator mLinkToTextCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        jniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);
        when(mDistillerUrlUtilsJniMock.getOriginalUrlFromDistillerUrl(any(String.class)))
                .thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL));
        ShareParams shareParamsWithLinkToText =
                new ShareParams.Builder(/*window=*/null, /*title=*/"", JUnitTestGURLs.EXAMPLE_URL)
                        .setText("text")
                        .build();
        when(mLinkToTextCoordinator.getShareParams(LinkToggleState.LINK))
                .thenReturn(shareParamsWithLinkToText);
        ShareParams shareParamsWithTextOnly =
                new ShareParams.Builder(/*window=*/null, /*title=*/"", /*url=*/"")
                        .setText("text")
                        .build();
        when(mLinkToTextCoordinator.getShareParams(LinkToggleState.NO_LINK))
                .thenReturn(shareParamsWithTextOnly);
    }

    @Test
    public void getShareParams_urlShare_sameShareParams() {
        ShareParams shareParams =
                new ShareParams.Builder(/*window=*/null, /*title=*/"", JUnitTestGURLs.EXAMPLE_URL)
                        .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(shareParams, chromeShareExtras, /*startTime=*/
                        0, /*linkToTextCoordinator=*/null, mShareCallback);

        assertEquals("ShareParams should be the same as the original.", shareParams,
                shareSheetLinkToggleCoordinator.getShareParams(LinkToggleState.NO_LINK));
    }

    @Test
    public void getShareParams_nonUrlShare_noLink() {
        ShareParams shareParams =
                new ShareParams.Builder(/*window=*/null, /*title=*/"", "").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setContentUrl(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL))
                        .build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(shareParams, chromeShareExtras, /*startTime=*/
                        0, /*linkToTextCoordinator=*/null, mShareCallback);

        assertTrue("ShareParams should not have a URL.",
                TextUtils.isEmpty(
                        shareSheetLinkToggleCoordinator.getShareParams(LinkToggleState.NO_LINK)
                                .getUrl()));
    }

    @Test
    public void getShareParams_nonUrlShare_link() {
        ShareParams shareParams =
                new ShareParams.Builder(/*window=*/null, /*title=*/"", "").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setContentUrl(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL))
                        .build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(shareParams, chromeShareExtras, /*startTime=*/
                        0, /*linkToTextCoordinator=*/null, mShareCallback);

        assertEquals("ShareParams should include the link.", JUnitTestGURLs.EXAMPLE_URL,
                shareSheetLinkToggleCoordinator.getShareParams(LinkToggleState.LINK).getUrl());
    }

    @Test
    public void getShareParams_linkToText_noLink() {
        ShareParams shareParams =
                new ShareParams.Builder(/*window=*/null, /*title=*/"", "").setText("text").build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder().build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(shareParams, chromeShareExtras, /*startTime=*/
                        0, mLinkToTextCoordinator, mShareCallback);

        assertTrue("ShareParams should not have a URL.",
                TextUtils.isEmpty(
                        shareSheetLinkToggleCoordinator.getShareParams(LinkToggleState.NO_LINK)
                                .getUrl()));
    }

    @Test
    public void getShareParams_linkToText_link() {
        ShareParams shareParams =
                new ShareParams.Builder(/*window=*/null, /*title=*/"", "").setText("text").build();
        ChromeShareExtras chromeShareExtras =
                new ChromeShareExtras.Builder().setIsUserHighlightedText(true).build();
        ShareSheetLinkToggleCoordinator shareSheetLinkToggleCoordinator =
                new ShareSheetLinkToggleCoordinator(shareParams, chromeShareExtras, /*startTime=*/
                        0, mLinkToTextCoordinator, mShareCallback);

        assertEquals("ShareParams should include the link.", JUnitTestGURLs.EXAMPLE_URL,
                shareSheetLinkToggleCoordinator.getShareParams(LinkToggleState.LINK).getUrl());
    }
}
