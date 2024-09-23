// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link LinkToTextCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LinkToTextCoordinatorTest {
    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock private ChromeOptionShareCallback mShareCallback;
    @Mock private WindowAndroid mWindow;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;
    @Mock private LinkToTextBridge.Natives mLinkToTextBridge;

    private LinkToTextCoordinator mLinkToTextCoordinator;
    private Activity mActivity;
    private ChromeShareExtras mChromeShareExtras;
    private ChromeShareExtras mReshareChromeShareExtras;
    private String mSelector;
    private Integer mError;
    private Integer mReadyStatus;
    private boolean mIsRemoteRequestResultSet;

    private static final String SELECTED_TEXT = "selection";
    private static final String VISIBLE_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    private static final String BLOCKLIST_URL = JUnitTestGURLs.URL_1.getSpec();
    private static final String SELECTED_TEXT_LONG =
            "This textbook has more freedom than most (but see some exceptions).";
    private static final long SHARE_START_TIME = 1L;

    private void checkShowsShareSheetWithNoLink() {
        ShareParams shareParams = mLinkToTextCoordinator.getShareParams(LinkToggleState.NO_LINK);
        verify(mShareCallback, times(1))
                .showShareSheet(eq(shareParams), any(), eq(SHARE_START_TIME));
        Assert.assertEquals("", shareParams.getUrl());
        Assert.assertEquals(false, shareParams.getLinkToTextSuccessful());
    }

    private void checkShowsShareSheetWithLink(String url) {
        ShareParams shareParams = mLinkToTextCoordinator.getShareParams(LinkToggleState.LINK);
        verify(mShareCallback, times(1))
                .showShareSheet(eq(shareParams), any(), eq(SHARE_START_TIME));
        Assert.assertEquals(url, shareParams.getUrl());
        Assert.assertEquals(true, shareParams.getLinkToTextSuccessful());
    }

    private void setGenerationRemoteRequestResults(
            String selector, Integer error, Integer readyStatus) {
        mIsRemoteRequestResultSet = true;
        mSelector = selector;
        mError = error;
        mReadyStatus = readyStatus;
    }

    private void setReshareRemoteRequestResults(String selector) {
        mIsRemoteRequestResultSet = true;
        mSelector = selector;
    }

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);
        when(mDistillerUrlUtilsJniMock.getOriginalUrlFromDistillerUrl(any(String.class)))
                .thenAnswer(
                        (invocation) -> {
                            return new GURL((String) invocation.getArguments()[0]);
                        });
        doNothing().when(mShareCallback).showShareSheet(any(), any(), anyLong());
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getWindowAndroid()).thenReturn(mWindow);
        when(mTab.getContext()).thenReturn(mActivity);

        jniMocker.mock(LinkToTextBridgeJni.TEST_HOOKS, mLinkToTextBridge);
        when(mLinkToTextBridge.shouldOfferLinkToText(any(GURL.class)))
                .thenAnswer(
                        (invocation) -> {
                            return !((GURL) invocation.getArguments()[0])
                                    .getSpec()
                                    .equals(BLOCKLIST_URL);
                        });
        mChromeShareExtras = new ChromeShareExtras.Builder().build();
        mReshareChromeShareExtras =
                new ChromeShareExtras.Builder().setIsReshareHighlightedText(true).build();

        mLinkToTextCoordinator =
                Mockito.spy(
                        new LinkToTextCoordinator() {
                            @Override
                            void requestSelector() {
                                // Consider solutions that will not leak implementation details to
                                // tests.
                                mLinkToTextCoordinator.mRemoteRequestStatus =
                                        RemoteRequestStatus.REQUESTED;
                                if (mIsRemoteRequestResultSet) {
                                    mLinkToTextCoordinator.onRemoteRequestCompleted(
                                            mSelector, mError, mReadyStatus);
                                }
                            }

                            @Override
                            void reshareHighlightedText() {
                                mLinkToTextCoordinator.mRemoteRequestStatus =
                                        RemoteRequestStatus.REQUESTED;
                                if (mIsRemoteRequestResultSet) {
                                    mLinkToTextCoordinator.reshareRequestCompleted(mSelector);
                                }
                            }
                        });
    }

    @Test
    @SmallTest
    public void showShareSheetTest_LinkGeneration() {
        String selector = "selector";
        String expectedUrlToShare = VISIBLE_URL + "#:~:text=selector";
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab,
                mShareCallback,
                mChromeShareExtras,
                SHARE_START_TIME,
                VISIBLE_URL,
                SELECTED_TEXT,
                false);
        mLinkToTextCoordinator.onSelectorReady(selector);
        checkShowsShareSheetWithLink(expectedUrlToShare);
    }

    @Test
    @SmallTest
    public void showShareSheetTest_UseLinkInTitle() {
        String selector = "selector";
        String expectedUrlToShare = VISIBLE_URL + "#:~:text=selector";
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab,
                mShareCallback,
                mChromeShareExtras,
                SHARE_START_TIME,
                VISIBLE_URL,
                SELECTED_TEXT,
                true);
        mLinkToTextCoordinator.onSelectorReady(selector);
        checkShowsShareSheetWithLink(expectedUrlToShare);
        Assert.assertEquals(
                "Title is different.",
                mActivity.getString(R.string.sharing_including_link_title_template, VISIBLE_URL),
                mLinkToTextCoordinator.getShareParams(LinkToggleState.LINK).getTitle());
    }

    @Test
    @SmallTest
    public void showShareSheetTest_LinkGenerationMultiHighlights() {
        String[] selectors = {"selector1", "selector2", "selector3"};
        String fragmentDirective =
                String.join(LinkToTextHelper.ADDITIONAL_TEXT_FRAGMENT_SELECTOR, selectors);
        String expectedUrlToShare =
                VISIBLE_URL + "#:~:text=selector1&text=selector2&text=selector3";

        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab,
                mShareCallback,
                mChromeShareExtras,
                SHARE_START_TIME,
                VISIBLE_URL,
                SELECTED_TEXT,
                false);
        mLinkToTextCoordinator.onSelectorReady(fragmentDirective);
        checkShowsShareSheetWithLink(expectedUrlToShare);
    }

    @Test
    @SmallTest
    public void showShareSheetTest_EmptySelector() {
        String selector = "";

        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab, mShareCallback, mChromeShareExtras, 1, VISIBLE_URL, SELECTED_TEXT, false);
        mLinkToTextCoordinator.onSelectorReady(selector);
        checkShowsShareSheetWithNoLink();
    }

    @Test
    @SmallTest
    public void getPreviewTextLongTest() {
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab,
                mShareCallback,
                mChromeShareExtras,
                SHARE_START_TIME,
                VISIBLE_URL,
                SELECTED_TEXT_LONG,
                false);
        Assert.assertEquals(
                "This textbook has more freedom t...", mLinkToTextCoordinator.getPreviewText());
    }

    @Test
    @SmallTest
    public void getPreviewTextTest() {
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab,
                mShareCallback,
                mChromeShareExtras,
                SHARE_START_TIME,
                VISIBLE_URL,
                SELECTED_TEXT,
                false);
        Assert.assertEquals("selection", mLinkToTextCoordinator.getPreviewText());
    }

    @Test
    @SmallTest
    public void shareLinkToTextTest_BlocklistUrl() {
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab,
                mShareCallback,
                mChromeShareExtras,
                SHARE_START_TIME,
                BLOCKLIST_URL,
                SELECTED_TEXT,
                false);
        mLinkToTextCoordinator.shareLinkToText();

        // Check that shows share sheet without link to text
        checkShowsShareSheetWithNoLink();

        // Check that histogram will be recorded correctly.
        verify(mLinkToTextBridge, times(1))
                .logFailureMetrics(any(), eq(LinkGenerationError.BLOCK_LIST));
    }

    @Test
    @SmallTest
    public void shareLinkToTextTest_GenerationError() {
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab, mShareCallback, mChromeShareExtras, SHARE_START_TIME, VISIBLE_URL, "", false);
        setGenerationRemoteRequestResults(
                "",
                Integer.valueOf(LinkGenerationError.EMPTY_SELECTION),
                Integer.valueOf(LinkGenerationReadyStatus.REQUESTED_AFTER_READY));
        mLinkToTextCoordinator.shareLinkToText();

        // Check that shows share sheet without link to text
        checkShowsShareSheetWithNoLink();

        // Check that histogram will be recorded correctly.
        verify(mLinkToTextBridge, times(1))
                .logFailureMetrics(any(), eq(LinkGenerationError.EMPTY_SELECTION));
    }

    @Test
    @SmallTest
    public void shareLinkToTextTest_Timeout_BeforeRemoteRequestComplete() {
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab, mShareCallback, mChromeShareExtras, SHARE_START_TIME, VISIBLE_URL, "", false);
        mLinkToTextCoordinator.shareLinkToText();
        mLinkToTextCoordinator.timeout();

        // Check that shows share sheet without link to text
        checkShowsShareSheetWithNoLink();

        // Check that histogram will be recorded correctly.
        verify(mLinkToTextBridge, times(1))
                .logFailureMetrics(any(), eq(LinkGenerationError.TIMEOUT));

        // Receiving generation result after timeout, should not trigger another sharesheet.
        mLinkToTextCoordinator.onRemoteRequestCompleted(
                "",
                Integer.valueOf(LinkGenerationError.EMPTY_SELECTION),
                Integer.valueOf(LinkGenerationReadyStatus.REQUESTED_BEFORE_READY));
        verify(mShareCallback, times(1)).showShareSheet(any(), any(), anyLong());

        // No new histogram is recorded.
        verify(mLinkToTextBridge, times(1)).logFailureMetrics(any(), anyInt());
        verify(mLinkToTextBridge, times(0)).logSuccessMetrics(any());
    }

    @Test
    @SmallTest
    public void shareLinkToTextTest_Timeout_AfterRemoteRequestComplete() {
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab, mShareCallback, mChromeShareExtras, SHARE_START_TIME, VISIBLE_URL, "", false);
        setGenerationRemoteRequestResults(
                "selector",
                Integer.valueOf(LinkGenerationError.NONE),
                Integer.valueOf(LinkGenerationReadyStatus.REQUESTED_AFTER_READY));
        mLinkToTextCoordinator.shareLinkToText();

        // Check that shows share sheet without link to text
        checkShowsShareSheetWithLink(VISIBLE_URL + "#:~:text=selector");
        verify(mLinkToTextBridge, times(1)).logSuccessMetrics(any());

        // Timeout after remote request was completed should not trigger another sharesheet.
        mLinkToTextCoordinator.timeout();
        verify(mShareCallback, times(1)).showShareSheet(any(), any(), anyLong());

        // No new histogram is recorded.
        verify(mLinkToTextBridge, times(0)).logFailureMetrics(any(), anyInt());
        verify(mLinkToTextBridge, times(1)).logSuccessMetrics(any());
    }

    @Test
    @SmallTest
    public void shareLinkToTextTest_OmniboxNavigation_BeforeRemoteRequestComplete() {
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab, mShareCallback, mChromeShareExtras, SHARE_START_TIME, VISIBLE_URL, "", false);
        mLinkToTextCoordinator.shareLinkToText();
        mLinkToTextCoordinator.onUpdateUrl(mTab, new GURL(VISIBLE_URL));

        // check doesn't show share sheet
        verify(mShareCallback, times(0)).showShareSheet(any(), any(), anyLong());

        // Check that histogram will be recorded correctly.
        verify(mLinkToTextBridge, times(1))
                .logFailureMetrics(any(), eq(LinkGenerationError.OMNIBOX_NAVIGATION));

        // Receiving generation result should not trigger another sharesheet.
        mLinkToTextCoordinator.onRemoteRequestCompleted(
                "",
                Integer.valueOf(LinkGenerationError.EMPTY_SELECTION),
                Integer.valueOf(LinkGenerationReadyStatus.REQUESTED_BEFORE_READY));
        verify(mShareCallback, times(0)).showShareSheet(any(), any(), anyLong());

        // No new histogram is recorded.
        verify(mLinkToTextBridge, times(1)).logFailureMetrics(any(), anyInt());
        verify(mLinkToTextBridge, times(0)).logSuccessMetrics(any());
    }

    @Test
    @SmallTest
    public void shareLinkToTextTest_Reshare_Success() {
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab,
                mShareCallback,
                mReshareChromeShareExtras,
                SHARE_START_TIME,
                VISIBLE_URL,
                "",
                false);
        setReshareRemoteRequestResults("selector");
        mLinkToTextCoordinator.shareLinkToText();

        // Check that shows share sheet without link to text
        checkShowsShareSheetWithLink(VISIBLE_URL + "#:~:text=selector");

        // Check that histogram will be recorded correctly.
        verify(mLinkToTextBridge, times(1))
                .logLinkToTextReshareStatus(LinkToTextReshareStatus.SUCCESS);
    }

    @Test
    @SmallTest
    public void shareLinkToTextTest_Reshare_Timeout_BeforeRemoteRequestComplete() {
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab,
                mShareCallback,
                mReshareChromeShareExtras,
                SHARE_START_TIME,
                VISIBLE_URL,
                "",
                false);
        mLinkToTextCoordinator.shareLinkToText();
        mLinkToTextCoordinator.timeout();

        // Check that shows share sheet without link to text
        checkShowsShareSheetWithNoLink();

        // Check that histogram will be recorded correctly.
        verify(mLinkToTextBridge, times(1))
                .logLinkToTextReshareStatus(LinkToTextReshareStatus.TIMEOUT);

        // Receiving generation result after timeout, should not trigger another sharesheet.
        mLinkToTextCoordinator.onReshareSelectorsRemoteRequestCompleted("");
        verify(mShareCallback, times(1)).showShareSheet(any(), any(), anyLong());

        // No new histogram is recorded.
        verify(mLinkToTextBridge, times(1)).logLinkToTextReshareStatus(anyInt());
    }

    @Test
    @SmallTest
    public void shareLinkToTextTest_Reshare_Timeout_AfterRemoteRequestComplete() {
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab,
                mShareCallback,
                mReshareChromeShareExtras,
                SHARE_START_TIME,
                VISIBLE_URL,
                "",
                false);
        setReshareRemoteRequestResults("selector");
        mLinkToTextCoordinator.shareLinkToText();

        // Check that shows share sheet without link to text
        checkShowsShareSheetWithLink(VISIBLE_URL + "#:~:text=selector");
        verify(mLinkToTextBridge, times(1))
                .logLinkToTextReshareStatus(LinkToTextReshareStatus.SUCCESS);

        // Timeout after remote request was completed should not trigger another sharesheet.
        mLinkToTextCoordinator.timeout();
        verify(mShareCallback, times(1)).showShareSheet(any(), any(), anyLong());

        // No new histogram is recorded.
        verify(mLinkToTextBridge, times(1)).logLinkToTextReshareStatus(anyInt());
    }

    @Test
    @SmallTest
    public void shareLinkToTextTest_Reshare_OmniboxNavigation_BeforeRemoteRequestComplete() {
        mLinkToTextCoordinator.initLinkToTextCoordinator(
                mTab,
                mShareCallback,
                mReshareChromeShareExtras,
                SHARE_START_TIME,
                VISIBLE_URL,
                "",
                false);
        mLinkToTextCoordinator.shareLinkToText();
        mLinkToTextCoordinator.onUpdateUrl(mTab, new GURL(VISIBLE_URL));

        // check doesn't show share sheet
        verify(mShareCallback, times(0)).showShareSheet(any(), any(), anyLong());

        // Check that histogram will be recorded correctly.
        verify(mLinkToTextBridge, times(1))
                .logLinkToTextReshareStatus(LinkToTextReshareStatus.OMNIBOX_NAVIGATION);

        // Receiving generation result should not trigger another sharesheet.
        mLinkToTextCoordinator.onReshareSelectorsRemoteRequestCompleted("");
        verify(mShareCallback, times(0)).showShareSheet(any(), any(), anyLong());

        // No new histogram is recorded.
        verify(mLinkToTextBridge, times(1)).logLinkToTextReshareStatus(anyInt());
    }
}
