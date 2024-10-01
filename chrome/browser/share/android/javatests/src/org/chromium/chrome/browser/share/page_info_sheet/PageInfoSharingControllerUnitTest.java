// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.share.page_info_sheet.PageSummaryMetrics.SHARE_SHEET_VISIBILITY_HISTOGRAM;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.view.View;
import android.widget.Button;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.hamcrest.MatcherAssert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.content_extraction.InnerTextBridge;
import org.chromium.chrome.browser.content_extraction.InnerTextBridgeJni;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.model_execution.ExecutionResult;
import org.chromium.chrome.browser.model_execution.ExecutionResult.ExecutionError;
import org.chromium.chrome.browser.model_execution.ModelExecutionSession;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.page_info_sheet.PageSummaryMetrics.PageSummarySheetEvents;
import org.chromium.chrome.browser.share.page_info_sheet.PageSummaryMetrics.ShareActionVisibility;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.widget.RadioButtonLayout;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.TextViewWithClickableSpans;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Optional;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.CHROME_SHARE_PAGE_INFO})
public class PageInfoSharingControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ModelExecutionSession mModelExecutionSession;
    @Mock private InnerTextBridge.Natives mInnerTextJniMock;
    @Mock private ChromeOptionShareCallback mChromeOptionShareCallback;
    @Mock private HelpAndFeedbackLauncher mMockFeedbackLauncher;
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
        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mMockFeedbackLauncher);
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

    private void setInnerTextExtractionResult(String innerText) {
        doAnswer(
                        invocationOnMock -> {
                            Callback<Optional<String>> innerTextCallback =
                                    invocationOnMock.getArgument(1);
                            innerTextCallback.onResult(Optional.of(innerText));
                            return null;
                        })
                .when(mInnerTextJniMock)
                .getInnerText(any(), any());
    }

    /**
     * Sets up and starts a summarization flow for {@code tab} and invokes the result callbacks that
     * move the UI into the success state, where the result can be shared and feedback can be
     * provided.
     */
    private void simulateSuccessfulSummarization(
            Context context, Tab tab, ArgumentCaptor<BottomSheetContent> bottomSheetContentCaptor) {

        ArgumentCaptor<Callback<ExecutionResult>> modelExecutionCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);

        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        setInnerTextExtractionResult("Inner text of web page");

        PageInfoSharingControllerImpl.getInstance()
                .sharePageInfo(context, mBottomSheetController, mChromeOptionShareCallback, tab);

        verify(mBottomSheetController)
                .requestShowContent(bottomSheetContentCaptor.capture(), anyBoolean());
        verify(mModelExecutionSession).executeModel(any(), modelExecutionCallbackCaptor.capture());
        Callback<ExecutionResult> executionResultCallback = modelExecutionCallbackCaptor.getValue();
        executionResultCallback.onResult(
                new ExecutionResult("Summary text", /* isCompleteResult= */ true));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CHROME_SHARE_PAGE_INFO})
    public void testShouldShow_withFeatureDisabled() {
        Tab tab = Mockito.mock(Tab.class);

        assertFalse(PageInfoSharingControllerImpl.getInstance().shouldShowInShareSheet(tab));
    }

    @Test
    public void testShouldShow_withNullTab() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SHARE_SHEET_VISIBILITY_HISTOGRAM,
                        ShareActionVisibility.NOT_SHOWN_TAB_NOT_VALID);
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        assertFalse(PageInfoSharingControllerImpl.getInstance().shouldShowInShareSheet(null));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testShouldShow_withNullUrl() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SHARE_SHEET_VISIBILITY_HISTOGRAM,
                        ShareActionVisibility.NOT_SHOWN_TAB_NOT_VALID);
        Tab tab = Mockito.mock(Tab.class);
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        when(tab.getUrl()).thenReturn(null);

        assertFalse(PageInfoSharingControllerImpl.getInstance().shouldShowInShareSheet(tab));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testShouldShow_withNonHttpUrl() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SHARE_SHEET_VISIBILITY_HISTOGRAM,
                        ShareActionVisibility.NOT_SHOWN_URL_NOT_VALID);
        Tab tab = createMockTab(JUnitTestGURLs.CHROME_ABOUT);
        when(mModelExecutionSession.isAvailable()).thenReturn(true);

        assertFalse(PageInfoSharingControllerImpl.getInstance().shouldShowInShareSheet(tab));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testShouldShow_withNonAvailableModel() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SHARE_SHEET_VISIBILITY_HISTOGRAM,
                        ShareActionVisibility.NOT_SHOWN_MODEL_NOT_AVAILABLE);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mModelExecutionSession.isAvailable()).thenReturn(false);

        assertFalse(PageInfoSharingControllerImpl.getInstance().shouldShowInShareSheet(tab));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testShouldShow_withAvailableModel() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SHARE_SHEET_VISIBILITY_HISTOGRAM, ShareActionVisibility.SHOWN);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(eq(mProfile))).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(eq(tab))).thenReturn(true);

        assertTrue(PageInfoSharingControllerImpl.getInstance().shouldShowInShareSheet(tab));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testShouldShow_withUnsupportedProfile() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SHARE_SHEET_VISIBILITY_HISTOGRAM,
                        ShareActionVisibility.NOT_SHOWN_PROFILE_NOT_SUPPORTED);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);

        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(false);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);

        assertFalse(PageInfoSharingControllerImpl.getInstance().shouldShowInShareSheet(tab));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testShouldShow_withUnsupportedLanguage() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SHARE_SHEET_VISIBILITY_HISTOGRAM,
                        ShareActionVisibility.NOT_SHOWN_TAB_NOT_SUPPORTED);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);

        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(false);

        assertFalse(PageInfoSharingControllerImpl.getInstance().shouldShowInShareSheet(tab));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testShouldShow_whileSharingAnotherTab() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SHARE_SHEET_VISIBILITY_HISTOGRAM,
                        ShareActionVisibility.NOT_SHOWN_ALREADY_RUNNING);
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        Tab firstTab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        Tab secondTab = createMockTab(JUnitTestGURLs.GOOGLE_URL);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(Mockito.any())).thenReturn(true);

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
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
                                    .shouldShowInShareSheet(secondTab));
                    histogramWatcher.assertExpected();
                });
    }

    @Test
    public void testSharePageInfo_ensureSheetOpens() {
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                        PageSummarySheetEvents.OPEN_SUMMARY_SHEET);

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
                activity -> {
                    PageInfoSharingControllerImpl.getInstance()
                            .sharePageInfo(
                                    activity,
                                    mBottomSheetController,
                                    mChromeOptionShareCallback,
                                    tab);
                    verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
                    histogramWatcher.assertExpected();
                });
    }

    @Test
    public void testDismissDialog_whileInitializing() {
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.OPEN_SUMMARY_SHEET)
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.CLOSE_SHEET_WHILE_INITIALIZING)
                        .build();

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
                activity -> {
                    ArgumentCaptor<BottomSheetContent> bottomSheetContentCaptor =
                            ArgumentCaptor.forClass(BottomSheetContent.class);

                    PageInfoSharingControllerImpl.getInstance()
                            .sharePageInfo(
                                    activity,
                                    mBottomSheetController,
                                    mChromeOptionShareCallback,
                                    tab);

                    verify(mBottomSheetController)
                            .requestShowContent(bottomSheetContentCaptor.capture(), anyBoolean());
                    BottomSheetContent sheetContent = bottomSheetContentCaptor.getValue();
                    View cancelButton =
                            sheetContent.getContentView().findViewById(R.id.cancel_button);

                    cancelButton.performClick();

                    verify(mBottomSheetController).hideContent(sheetContent, true);
                    histogramWatcher.assertExpected();
                });
    }

    @Test
    public void testDismissDialog_whileLoading() {
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);
        setInnerTextExtractionResult("Inner text of web page");
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.OPEN_SUMMARY_SHEET)
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.CLOSE_SHEET_WHILE_LOADING)
                        .build();

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
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
                            .requestShowContent(bottomSheetContentCaptor.capture(), anyBoolean());
                    verify(mModelExecutionSession)
                            .executeModel(anyString(), modelExecutionCallbackCaptor.capture());

                    Callback<ExecutionResult> executionResultCallback =
                            modelExecutionCallbackCaptor.getValue();

                    // Call model execution callback with a single partial streaming result.
                    executionResultCallback.onResult(
                            new ExecutionResult("Test", /* isCompleteResult= */ false));
                    ShadowLooper.runUiThreadTasks();

                    BottomSheetContent sheetContent = bottomSheetContentCaptor.getValue();
                    View cancelButton =
                            sheetContent.getContentView().findViewById(R.id.cancel_button);

                    cancelButton.performClick();

                    verify(mBottomSheetController).hideContent(sheetContent, true);
                    histogramWatcher.assertExpected();
                });
    }

    @Test
    public void testDismissDialog_afterError() {
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);
        setInnerTextExtractionResult("Inner text of web page");
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.OPEN_SUMMARY_SHEET)
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.CLOSE_SHEET_ON_ERROR)
                        .build();

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
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
                            .requestShowContent(bottomSheetContentCaptor.capture(), anyBoolean());
                    verify(mModelExecutionSession)
                            .executeModel(anyString(), modelExecutionCallbackCaptor.capture());

                    Callback<ExecutionResult> executionResultCallback =
                            modelExecutionCallbackCaptor.getValue();

                    // Call model execution callback with an error.
                    executionResultCallback.onResult(
                            new ExecutionResult(ExecutionError.NOT_AVAILABLE));
                    ShadowLooper.runUiThreadTasks();

                    BottomSheetContent sheetContent = bottomSheetContentCaptor.getValue();
                    View cancelButton =
                            sheetContent.getContentView().findViewById(R.id.cancel_button);

                    cancelButton.performClick();

                    verify(mBottomSheetController).hideContent(sheetContent, true);
                    histogramWatcher.assertExpected();
                });
    }

    @Test
    public void testDismissDialog_afterSuccessfulLoading() {
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.OPEN_SUMMARY_SHEET)
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.CLOSE_SHEET_AFTER_SUCCESS)
                        .build();

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
                activity -> {
                    ArgumentCaptor<BottomSheetContent> bottomSheetContentCaptor =
                            ArgumentCaptor.forClass(BottomSheetContent.class);
                    simulateSuccessfulSummarization(activity, tab, bottomSheetContentCaptor);
                    ShadowLooper.runUiThreadTasks();

                    BottomSheetContent sheetContent = bottomSheetContentCaptor.getValue();
                    View cancelButton =
                            sheetContent.getContentView().findViewById(R.id.cancel_button);

                    cancelButton.performClick();

                    verify(mBottomSheetController).hideContent(sheetContent, true);
                    histogramWatcher.assertExpected();
                });
    }

    @Test
    public void testSharePageInfo_ensurePageContentsAreRequestedAndSentToSummarization() {
        when(mModelExecutionSession.isAvailable()).thenReturn(true);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        when(mPageInfoSharingBridgeJni.doesProfileSupportPageInfo(mProfile)).thenReturn(true);
        when(mPageInfoSharingBridgeJni.doesTabSupportPageInfo(tab)).thenReturn(true);
        RenderFrameHost mainFrame = tab.getWebContents().getMainFrame();

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
                activity -> {
                    ArgumentCaptor<Callback<Optional<String>>> innerTextExtractionCallbackCaptor =
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
                                    eq(mainFrame), innerTextExtractionCallbackCaptor.capture());

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

        setInnerTextExtractionResult("Inner text of web page");

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
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
                            .requestShowContent(bottomSheetContentCaptor.capture(), anyBoolean());
                    verify(mModelExecutionSession)
                            .executeModel(
                                    eq("Inner text of web page"),
                                    modelExecutionCallbackCaptor.capture());

                    Callback<ExecutionResult> executionResultCallback =
                            modelExecutionCallbackCaptor.getValue();
                    BottomSheetContent bottomSheetContent = bottomSheetContentCaptor.getValue();

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
                            bottomSheetContent.getContentView().findViewById(R.id.summary_text);
                    View acceptButton =
                            bottomSheetContent.getContentView().findViewById(R.id.accept_button);

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
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.OPEN_SUMMARY_SHEET)
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.ADD_SUMMARY)
                        .build();

        setInnerTextExtractionResult("Inner text of web page");

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
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
                            .requestShowContent(bottomSheetContentCaptor.capture(), anyBoolean());
                    verify(mModelExecutionSession)
                            .executeModel(
                                    eq("Inner text of web page"),
                                    modelExecutionCallbackCaptor.capture());

                    Callback<ExecutionResult> executionResultCallback =
                            modelExecutionCallbackCaptor.getValue();
                    BottomSheetContent bottomSheetContent = bottomSheetContentCaptor.getValue();
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
                            new ExecutionResult("Page summary", /* isCompleteResult= */ true));

                    View summaryTextView =
                            bottomSheetContent.getContentView().findViewById(R.id.summary_text);
                    View acceptButton =
                            bottomSheetContent.getContentView().findViewById(R.id.accept_button);

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
                    assertEquals(tab.getUrl().getSpec(), shareParamsCaptor.getValue().getUrl());
                    assertEquals("Page summary", shareParamsCaptor.getValue().getText());
                    assertEquals(
                            DetailedContentType.PAGE_INFO,
                            chromeShareExtrasCaptor.getValue().getDetailedContentType());
                    histogramWatcher.assertExpected();
                });
    }

    @Test
    public void testNegativeFeedback() {
        ArgumentCaptor<BottomSheetContent> sheetContentCaptor =
                ArgumentCaptor.forClass(BottomSheetContent.class);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        String tabUrl = tab.getUrl().getSpec();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.OPEN_SUMMARY_SHEET)
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.CLICK_NEGATIVE_FEEDBACK)
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.NEGATIVE_FEEDBACK_TYPE_SELECTED)
                        .build();

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
                activity -> {
                    simulateSuccessfulSummarization(activity, tab, sheetContentCaptor);

                    BottomSheetContent summaryBottomSheetContent = sheetContentCaptor.getValue();

                    // Summary bottom sheet has positive and negative feedback buttons, get
                    // the negative one and click it.
                    View negativeFeedbackButton =
                            summaryBottomSheetContent
                                    .getContentView()
                                    .findViewById(R.id.negative_feedback_button);
                    negativeFeedbackButton.performClick();

                    // Summary sheet should be hidden, and a new sheet with feedback options
                    // should be shown.
                    verify(mBottomSheetController).hideContent(summaryBottomSheetContent, true);
                    verify(mBottomSheetController, times(2))
                            .requestShowContent(sheetContentCaptor.capture(), eq(true));

                    // Get feedback form and accept button from feedback sheet.
                    BottomSheetContent feedbackBottomSheetContent = sheetContentCaptor.getValue();
                    RadioButtonLayout radioButtons =
                            feedbackBottomSheetContent
                                    .getContentView()
                                    .findViewById(R.id.radio_buttons);
                    Button acceptFeedbackButton =
                            feedbackBottomSheetContent
                                    .getContentView()
                                    .findViewById(R.id.accept_button);

                    // Ensure radio button group is not empty.
                    assertNotEquals(0, radioButtons.getChildCount());
                    // Click on first radio button item (offensive result).
                    radioButtons.selectChildAtIndex(0);
                    acceptFeedbackButton.performClick();

                    // Feedback sheet should be hidden.
                    verify(mBottomSheetController).hideContent(feedbackBottomSheetContent, true);

                    // Feedback launcher should have been invoked.
                    verify(mMockFeedbackLauncher)
                            .showFeedback(eq(activity), eq(tabUrl), anyString(), any());
                    histogramWatcher.assertExpected();
                });
    }

    @Test
    public void testNegativeFeedback_cancelFeedbackSheet() {
        ArgumentCaptor<BottomSheetContent> sheetContentCaptor =
                ArgumentCaptor.forClass(BottomSheetContent.class);
        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.OPEN_SUMMARY_SHEET)
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.CLICK_NEGATIVE_FEEDBACK)
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.NEGATIVE_FEEDBACK_SHEET_DISMISSED)
                        .build();

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
                activity -> {
                    simulateSuccessfulSummarization(activity, tab, sheetContentCaptor);

                    BottomSheetContent summaryBottomSheetContent = sheetContentCaptor.getValue();

                    // Summary bottom sheet has positive and negative feedback buttons, get
                    // the negative one and click it.
                    View negativeFeedbackButton =
                            summaryBottomSheetContent
                                    .getContentView()
                                    .findViewById(R.id.negative_feedback_button);
                    negativeFeedbackButton.performClick();

                    // Summary sheet should be hidden, and a new sheet with feedback options
                    // should be shown.
                    verify(mBottomSheetController).hideContent(summaryBottomSheetContent, true);
                    verify(mBottomSheetController, times(2))
                            .requestShowContent(sheetContentCaptor.capture(), eq(true));

                    // Get feedback form and accept button from feedback sheet.
                    BottomSheetContent feedbackBottomSheetContent = sheetContentCaptor.getValue();
                    Button cancelFeedbackButton =
                            feedbackBottomSheetContent
                                    .getContentView()
                                    .findViewById(R.id.cancel_button);

                    // Select an item and click accept.
                    cancelFeedbackButton.performClick();

                    // Feedback sheet should be hidden.
                    verify(mBottomSheetController).hideContent(feedbackBottomSheetContent, true);
                    // Loading sheet should be shown again.
                    verify(mBottomSheetController, times(3))
                            .requestShowContent(sheetContentCaptor.capture(), eq(true));
                    MatcherAssert.assertThat(
                            sheetContentCaptor.getValue(),
                            is(instanceOf(PageInfoBottomSheetContent.class)));

                    // Feedback launcher shouldn't have been invoked.
                    verifyNoInteractions(mMockFeedbackLauncher);
                    histogramWatcher.assertExpected();
                });
    }

    @Test
    public void testOpenLearnMoreLink() {
        String testLearnMoreUrl = "https://google.com/learn_more";
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CHROME_SHARE_PAGE_INFO, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CHROME_SHARE_PAGE_INFO,
                PageSummarySharingRequest.LEARN_MORE_URL_PARAM,
                testLearnMoreUrl);
        FeatureList.setTestValues(testValues);

        ArgumentCaptor<BottomSheetContent> sheetContentCaptor =
                ArgumentCaptor.forClass(BottomSheetContent.class);

        Tab tab = createMockTab(JUnitTestGURLs.EXAMPLE_URL);
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.OPEN_SUMMARY_SHEET)
                        .expectIntRecord(
                                PageSummaryMetrics.SUMMARY_SHEET_UI_EVENTS,
                                PageSummarySheetEvents.CLICK_LEARN_MORE)
                        .build();

        var activityScenario = mActivityScenarioRule.getScenario();
        activityScenario.onActivity(
                activity -> {
                    simulateSuccessfulSummarization(activity, tab, sheetContentCaptor);

                    BottomSheetContent summaryBottomSheetContent = sheetContentCaptor.getValue();

                    // Summary bottom sheet has a disclaimer inside a TextView with clickable spans.
                    TextViewWithClickableSpans learnMoreText =
                            summaryBottomSheetContent
                                    .getContentView()
                                    .findViewById(R.id.learn_more_text);
                    var learnMoreTextLinks = learnMoreText.getClickableSpans();
                    assertNotEquals(
                            "TextView should contain clickable spans",
                            0,
                            learnMoreTextLinks.length);
                    // Click first span, which should contain a "learn more" text and link to a web
                    // page.
                    learnMoreTextLinks[0].onClick(learnMoreText);

                    // Link should have opened in a CCT.
                    ShadowActivity shadowActivity = shadowOf(activity);
                    var launchedIntent = shadowActivity.getNextStartedActivity();
                    assertEquals(Intent.ACTION_VIEW, launchedIntent.getAction());
                    assertEquals(Uri.parse(testLearnMoreUrl), launchedIntent.getData());

                    // Summary sheet should be closed.
                    verify(mBottomSheetController).hideContent(summaryBottomSheetContent, true);
                    histogramWatcher.assertExpected();
                });
    }
}
