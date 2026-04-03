// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.AutofillSheetUiController;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetMediator.VirtualCardEnrollmentBubbleResult;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit test for {@link AutofillVcnEnrollBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillVcnEnrollBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutofillVcnEnrollBottomSheetContent mContent;
    @Mock private AutofillVcnEnrollBottomSheetLifecycle mLifecycle;
    @Mock private AutofillSheetUiController mUiController;
    private PropertyModel mModel;
    private AutofillVcnEnrollBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(AutofillVcnEnrollBottomSheetProperties.ALL_KEYS).build();
        when(mLifecycle.canBegin()).thenReturn(true);
        when(mUiController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator =
                new AutofillVcnEnrollBottomSheetMediator(
                        mContent, mLifecycle, mUiController, mModel);
    }

    @Test
    public void testShowBottomSheet() {
        mMediator.requestShowContent();

        verify(mUiController)
                .requestShowContent(/* content= */ eq(mContent), /* animate= */ eq(true));
    }

    @Test
    public void testCannotShowBottomSheet() {
        when(mLifecycle.canBegin()).thenReturn(false); // E.g., when in tab overview.

        mMediator.requestShowContent();

        verifyNoInteractions(mUiController);
    }

    @Test
    public void testOnAccept_showsLoadingState() {
        mMediator.requestShowContent();
        mMediator.onAccept();

        assertTrue(mModel.get(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE));
        verify(mUiController, times(0)).hideContent(any(), anyBoolean(), anyInt());
    }

    @Test
    public void testOnCancel() {
        mMediator.requestShowContent();
        mMediator.onCancel();

        verify(mUiController)
                .hideContent(
                        eq(mContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testHideBottomSheetAfterShowing() {
        mMediator.requestShowContent();
        mMediator.hide();

        verify(mUiController)
                .hideContent(
                        /* content= */ eq(mContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testHideBottomSheetWithoutShowing() {
        mMediator.hide();

        verifyNoInteractions(mUiController);
    }

    @Test
    public void testMetrics_hideAfterOnAccept_RecordsMetric() {
        HistogramWatcher loadingShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillVcnEnrollBottomSheetMediator.LOADING_SHOWN_HISTOGRAM, true);
        HistogramWatcher loadingResultHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillVcnEnrollBottomSheetMediator.LOADING_RESULT_HISTOGRAM,
                        VirtualCardEnrollmentBubbleResult.ACCEPTED);

        mMediator.requestShowContent();
        mMediator.onAccept();
        mMediator.hide();

        loadingShownHistogram.assertExpected();
        loadingResultHistogram.assertExpected();
    }

    @Test
    public void testMetrics_onCanceledAfterOnAccept_RecordsClosedLoadingResult() {
        HistogramWatcher loadingResultHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillVcnEnrollBottomSheetMediator.LOADING_RESULT_HISTOGRAM,
                        VirtualCardEnrollmentBubbleResult.CLOSED);

        mMediator.requestShowContent();
        mMediator.onAccept();
        mMediator.onCancel();

        loadingResultHistogram.assertExpected();
    }

    @Test
    public void testMetrics_hideWithoutCallbacks_NoRecords() {
        HistogramWatcher loadingResultHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                AutofillVcnEnrollBottomSheetMediator.LOADING_RESULT_HISTOGRAM)
                        .build();

        mMediator.requestShowContent();
        mMediator.hide();

        loadingResultHistogram.assertExpected();
    }
}
