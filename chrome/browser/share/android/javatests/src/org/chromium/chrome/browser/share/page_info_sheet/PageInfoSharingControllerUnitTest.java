// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.content_extraction.InnerTextBridge;
import org.chromium.chrome.browser.content_extraction.InnerTextBridgeJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.model_execution.ExecutionResult;
import org.chromium.chrome.browser.model_execution.ModelExecutionSession;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Optional;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.CHROME_SHARE_PAGE_INFO})
public class PageInfoSharingControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ModelExecutionSession mModelExecutionSession;
    @Mock private InnerTextBridge.Natives mInnerTextJniMock;
    @Mock private ChromeOptionShareCallback mChromeOptionShareCallback;
    @Mock private PageInfoSharingBridge.Natives mPageInfoSharingBridgeJni;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private Profile mProfile;

    private Tab createMockTab(GURL url) {
        Tab tab = mock(Tab.class);
        when(tab.getUrl()).thenReturn(url);
        when(tab.getProfile()).thenReturn(mProfile);
        when(tab.getWebContents()).thenReturn(mock(WebContents.class));
        when(tab.getWebContents().getMainFrame()).thenReturn(mock(RenderFrameHost.class));

        return tab;
    }

    @Before
    public void setUp() {
        PageInfoSharingControllerImpl.resetForTesting();
        mJniMocker.mock(InnerTextBridgeJni.TEST_HOOKS, mInnerTextJniMock);
        mJniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDomDistillerUrlUtilsJni);
        mJniMocker.mock(PageInfoSharingBridgeJni.TEST_HOOKS, mPageInfoSharingBridgeJni);
        when(mDomDistillerUrlUtilsJni.getOriginalUrlFromDistillerUrl(anyString()))
                .thenAnswer(
                        (invocation) -> {
                            return new GURL((String) invocation.getArguments()[0]);
                        });
        ((PageInfoSharingControllerImpl) PageInfoSharingControllerImpl.getInstance())
                .setModelExecutionSessionForTesting(mModelExecutionSession);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CHROME_SHARE_PAGE_INFO})
    public void testIsAvailable_withFeatureDisabled() {
        Tab tab = Mockito.mock(Tab.class);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withNullTab() {
        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(null));
    }

    @Test
    public void testIsAvailable_withNullUrl() {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.getUrl()).thenReturn(null);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withNonHttpUrl() {
        Tab tab = createMockTab(JUnitTestGURLs.CHROME_ABOUT);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withNonAvailableModel() {
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mModelExecutionSession.isAvailable()).thenReturn(false);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withAvailableModel() {
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(eq(mProfile))).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(eq(tab))).thenReturn(true);

        assertTrue(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withUnsupportedProfile() {
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);

        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(false);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_withUnsupportedLanguage() {
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);

        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(false);

        assertFalse(PageInfoSharingControllerImpl.getInstance().isAvailableForTab(tab));
    }

    @Test
    public void testIsAvailable_whileSharingAnotherTab() {
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        Tab firstTab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        Tab secondTab = createMockTab(JUnitTestGURLs.GOOGLE_URL);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(Mockito.any())).thenReturn(true);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            PageInfoSharingControllerImpl.getInstance()
                                    .sharePageInfo(
                                            activity,
                                            mBottomSheetController,
                                            mChromeOptionShareCallback,
                                            firstTab);
                            assertFalse(
                                    "Page sharing process should only happen for one tab at a time",
                                    PageInfoSharingControllerImpl.getInstance()
                                            .isAvailableForTab(secondTab));
                        });
    }

    @Test
    public void testSharePageInfo_ensureSheetOpens() {
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            PageInfoSharingControllerImpl.getInstance()
                                    .sharePageInfo(
                                            activity,
                                            mBottomSheetController,
                                            mChromeOptionShareCallback,
                                            tab);
                            verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
                        });
    }

    @Test
    public void testSharePageInfo_ensurePageContentsAreRequestedAndSentToSummarization() {
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);
        RenderFrameHost mainFrame = tab.getWebContents().getMainFrame();

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            ArgumentCaptor<Callback<Optional<String>>>
                                    innerTextExtractionCallbackCaptor =
                                            ArgumentCaptor.forClass(Callback.class);

                            PageInfoSharingControllerImpl.getInstance()
                                    .sharePageInfo(
                                            activity,
                                            mBottomSheetController,
                                            mChromeOptionShareCallback,
                                            tab);

                            // Verify page text extraction was requested.
                            verify(mInnerTextJniMock)
                                    .getInnerText(
                                            eq(mainFrame),
                                            innerTextExtractionCallbackCaptor.capture());

                            innerTextExtractionCallbackCaptor
                                    .getValue()
                                    .onResult(Optional.of("Inner text of web page"));

                            verify(mModelExecutionSession)
                                    .executeModel(eq("Inner text of web page"), any());
                        });
    }

    @Test
    public void testSharePageInfo_ensureSummaryIsStreamedToSheet() {
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);
        RenderFrameHost mainFrame = tab.getWebContents().getMainFrame();

        doAnswer(
                        invocationOnMock -> {
                            Callback<Optional<String>> innerTextCallback =
                                    invocationOnMock.getArgument(1);
                            innerTextCallback.onResult(Optional.of("Inner text of web page"));
                            return null;
                        })
                .when(mInnerTextJniMock)
                .getInnerText(eq(mainFrame), any());

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            ArgumentCaptor<Callback<ExecutionResult>> modelExecutionCallbackCaptor =
                                    ArgumentCaptor.forClass(Callback.class);
                            ArgumentCaptor<BottomSheetContent> bottomSheetContentCaptor =
                                    ArgumentCaptor.forClass(BottomSheetContent.class);

                            PageInfoSharingControllerImpl.getInstance()
                                    .sharePageInfo(
                                            activity,
                                            mBottomSheetController,
                                            mChromeOptionShareCallback,
                                            tab);

                            verify(mBottomSheetController)
                                    .requestShowContent(
                                            bottomSheetContentCaptor.capture(), anyBoolean());
                            verify(mModelExecutionSession)
                                    .executeModel(
                                            eq("Inner text of web page"),
                                            modelExecutionCallbackCaptor.capture());

                            Callback<ExecutionResult> executionResultCallback =
                                    modelExecutionCallbackCaptor.getValue();
                            BottomSheetContent bottomSheetContent =
                                    bottomSheetContentCaptor.getValue();

                            // Call model execution callback with multiple streaming results.
                            executionResultCallback.onResult(
                                    new ExecutionResult("Web", /* isCompleteResult= */ false));
                            executionResultCallback.onResult(
                                    new ExecutionResult(" ", /* isCompleteResult= */ false));
                            executionResultCallback.onResult(
                                    new ExecutionResult("page", /* isCompleteResult= */ false));
                            executionResultCallback.onResult(
                                    new ExecutionResult(" ", /* isCompleteResult= */ false));
                            executionResultCallback.onResult(
                                    new ExecutionResult("sum", /* isCompleteResult= */ false));
                            executionResultCallback.onResult(
                                    new ExecutionResult("mary", /* isCompleteResult= */ false));

                            View summaryTextView =
                                    bottomSheetContent
                                            .getContentView()
                                            .findViewById(R.id.summary_text);
                            View acceptButton =
                                    bottomSheetContent
                                            .getContentView()
                                            .findViewById(R.id.accept_button);

                            ShadowLooper.runUiThreadTasks();

                            // Streaming results should be concatenated and shown on the bottom
                            // sheet.
                            ViewMatchers.assertThat(summaryTextView, withText("Web page summary"));
                            // Accept button should be hidden during streaming.
                            ViewMatchers.assertThat(acceptButton, not(ViewMatchers.isDisplayed()));
                        });
    }

    @Test
    public void testSharePageInfo_ensureFinalResultCanBeShared() {
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);
        RenderFrameHost mainFrame = tab.getWebContents().getMainFrame();

        doAnswer(
                        invocationOnMock -> {
                            Callback<Optional<String>> innerTextCallback =
                                    invocationOnMock.getArgument(1);
                            innerTextCallback.onResult(Optional.of("Inner text of web page"));
                            return null;
                        })
                .when(mInnerTextJniMock)
                .getInnerText(eq(mainFrame), any());

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            ArgumentCaptor<Callback<ExecutionResult>> modelExecutionCallbackCaptor =
                                    ArgumentCaptor.forClass(Callback.class);
                            ArgumentCaptor<BottomSheetContent> bottomSheetContentCaptor =
                                    ArgumentCaptor.forClass(BottomSheetContent.class);

                            PageInfoSharingControllerImpl.getInstance()
                                    .sharePageInfo(
                                            activity,
                                            mBottomSheetController,
                                            mChromeOptionShareCallback,
                                            tab);

                            verify(mBottomSheetController)
                                    .requestShowContent(
                                            bottomSheetContentCaptor.capture(), anyBoolean());
                            verify(mModelExecutionSession)
                                    .executeModel(
                                            eq("Inner text of web page"),
                                            modelExecutionCallbackCaptor.capture());

                            Callback<ExecutionResult> executionResultCallback =
                                    modelExecutionCallbackCaptor.getValue();
                            BottomSheetContent bottomSheetContent =
                                    bottomSheetContentCaptor.getValue();
                            ArgumentCaptor<ShareParams> shareParamsCaptor =
                                    ArgumentCaptor.forClass(ShareParams.class);
                            ArgumentCaptor<ChromeShareExtras> chromeShareExtrasCaptor =
                                    ArgumentCaptor.forClass(ChromeShareExtras.class);

                            // Call model execution callback with multiple streaming results and a
                            // final result.
                            executionResultCallback.onResult(
                                    new ExecutionResult("Page", /* isCompleteResult= */ false));
                            executionResultCallback.onResult(
                                    new ExecutionResult(" ", /* isCompleteResult= */ false));
                            executionResultCallback.onResult(
                                    new ExecutionResult("summary", /* isCompleteResult= */ false));
                            executionResultCallback.onResult(
                                    new ExecutionResult(
                                            "Page summary", /* isCompleteResult= */ true));

                            View summaryTextView =
                                    bottomSheetContent
                                            .getContentView()
                                            .findViewById(R.id.summary_text);
                            View acceptButton =
                                    bottomSheetContent
                                            .getContentView()
                                            .findViewById(R.id.accept_button);

                            ShadowLooper.runUiThreadTasks();

                            // Final result should be visible on sheet.
                            ViewMatchers.assertThat(summaryTextView, withText("Page summary"));
                            // Accept button should enabled after receiving final result.
                            ViewMatchers.assertThat(acceptButton, isEnabled());

                            // Click accept button.
                            acceptButton.performClick();

                            // Verify share sheet was opened.
                            verify(mChromeOptionShareCallback)
                                    .showShareSheet(
                                            shareParamsCaptor.capture(),
                                            chromeShareExtrasCaptor.capture(),
                                            anyLong());

                            // Ensure shared params contains URL and summary.
                            assertEquals(
                                    tab.getUrl().getSpec(), shareParamsCaptor.getValue().getUrl());
                            assertEquals("Page summary", shareParamsCaptor.getValue().getText());
                            assertEquals(
                                    DetailedContentType.PAGE_INFO,
                                    chromeShareExtrasCaptor.getValue().getDetailedContentType());
                        });
    }
}
