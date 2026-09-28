// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiSourceAttributionProperties.HeaderProperties;
import org.chromium.components.autofill.autofill_ai.AutofillAiSourceAttributionInfo;
import org.chromium.components.autofill.autofill_ai.SourceType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.url.GURL;

import java.util.List;

/** Unit tests for {@link AutofillAiSourceAttributionCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillAiSourceAttributionCoordinatorTest {
    private static final String TEST_SUBTITLE = "Vehicle · AN-147338";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Runnable mOnDismissedCallback;

    private TestActivity mActivity;
    private AutofillAiSourceAttributionCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        List<AutofillAiSourceAttributionInfo> sources =
                List.of(
                        new AutofillAiSourceAttributionInfo(
                                SourceType.GMAIL,
                                new GURL("https://mail.google.com"),
                                "Flight reservation"));
        mCoordinator =
                new AutofillAiSourceAttributionCoordinator(
                        mActivity,
                        mBottomSheetController,
                        sources,
                        TEST_SUBTITLE,
                        mOnDismissedCallback);
    }

    @Test
    public void testModelInitialization() {
        ModelList modelList = mCoordinator.getModelListForTesting();
        assertEquals(2, modelList.size());
        assertEquals(TEST_SUBTITLE, modelList.get(0).model.get(HeaderProperties.SUBTITLE));
    }

    @Test
    public void testRequestShowContent_success() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Autofill.Ai.AttributionSheet.Shown", true);
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(true);
        assertTrue(mCoordinator.requestShowContent());
        assertTrue(mCoordinator.isShowingForTesting());
        verify(mBottomSheetController).addObserver(mCoordinator.getSheetObserverForTesting());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRequestShowContent_failureRetractsAndDestroys() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.Ai.AttributionSheet.Shown", false);
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(false);
        assertFalse(mCoordinator.requestShowContent());
        assertFalse(mCoordinator.isShowingForTesting());
        verify(mBottomSheetController, never()).addObserver(any());
        verify(mBottomSheetController, never()).removeObserver(any());
        verify(mBottomSheetController, never()).hideContent(any(), anyBoolean());
        verify(mBottomSheetController, never()).hideContent(any(), anyBoolean(), anyInt());
        verify(mOnDismissedCallback).run();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDestroy_whenNotShown() {
        mCoordinator.destroy();
        assertFalse(mCoordinator.isShowingForTesting());
        verify(mBottomSheetController, never()).removeObserver(any());
        verify(mBottomSheetController, never()).hideContent(any(), anyBoolean(), anyInt());
        verify(mOnDismissedCallback).run();
    }

    @Test
    public void testDestroy_isIdempotent() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(true);
        mCoordinator.requestShowContent();

        mCoordinator.destroy();
        assertFalse(mCoordinator.isShowingForTesting());

        // Repeated call should be a no-op
        mCoordinator.destroy();
        verify(mOnDismissedCallback, times(1)).run();
    }

    @Test
    public void testDestroy_afterFailedShow_idempotent() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(false);
        mCoordinator.requestShowContent();

        mCoordinator.destroy();
        verify(mOnDismissedCallback, times(1)).run();
    }

    @Test
    public void testOnSheetClosed_nonMatchingContent_ignored() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(true);
        mCoordinator.requestShowContent();

        when(mBottomSheetController.getCurrentSheetContent())
                .thenReturn(mock(BottomSheetContent.class));
        mCoordinator
                .getSheetObserverForTesting()
                .onSheetClosed(StateChangeReason.INTERACTION_COMPLETE);
        assertTrue(mCoordinator.isShowingForTesting());
        verify(mOnDismissedCallback, never()).run();
    }

    @Test
    public void testOnSheetClosed_matchingContent_triggersDestroy() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.Ai.AttributionSheet.Dismissed",
                        StateChangeReason.INTERACTION_COMPLETE);
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(true);
        mCoordinator.requestShowContent();

        when(mBottomSheetController.getCurrentSheetContent())
                .thenReturn(mCoordinator.getContentForTesting());
        mCoordinator
                .getSheetObserverForTesting()
                .onSheetClosed(StateChangeReason.INTERACTION_COMPLETE);
        assertFalse(mCoordinator.isShowingForTesting());
        verify(mOnDismissedCallback).run();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDestroy_removesObserverBeforeHideContent() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(true);
        mCoordinator.requestShowContent();

        InOrder inOrder = Mockito.inOrder(mBottomSheetController);
        mCoordinator.destroy();

        inOrder.verify(mBottomSheetController)
                .removeObserver(mCoordinator.getSheetObserverForTesting());
        inOrder.verify(mBottomSheetController)
                .hideContent(
                        eq(mCoordinator.getContentForTesting()),
                        eq(false),
                        eq(StateChangeReason.NONE));
        assertFalse(mCoordinator.isShowingForTesting());
        verify(mOnDismissedCallback).run();
    }
}
