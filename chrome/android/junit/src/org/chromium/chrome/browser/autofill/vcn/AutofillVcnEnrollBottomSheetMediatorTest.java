// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetMediator.VirtualCardEnrollmentBubbleResult;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit test for {@link AutofillVcnEnrollBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillVcnEnrollBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutofillVcnEnrollBottomSheetContent mContent;
    @Mock private AutofillVcnEnrollBottomSheetLifecycle mLifecycle;
    @Mock private ManagedBottomSheetController mBottomSheetController;
    private PropertyModel mModel;
    private WindowAndroid mWindow;
    private AutofillVcnEnrollBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mModel = new PropertyModel.Builder(AutofillVcnEnrollBottomSheetProperties.ALL_KEYS).build();
        mWindow = new WindowAndroid(activity);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        when(mLifecycle.canBegin()).thenReturn(true);
        mMediator = new AutofillVcnEnrollBottomSheetMediator(mContent, mLifecycle, mModel);
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    public void testShowBottomSheet() {
        mMediator.requestShowContent(mWindow);

        verify(mBottomSheetController)
                .requestShowContent(/* content= */ eq(mContent), /* animate= */ eq(true));
    }

    @Test
    public void testCannotShowBottomSheet() {
        when(mLifecycle.canBegin()).thenReturn(false); // E.g., when in tab overview.

        mMediator.requestShowContent(mWindow);

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VCN_ENROLL_LOADING_AND_CONFIRMATION})
    public void testOnAccept_showsLoadingState() {
        mMediator.requestShowContent(mWindow);
        mMediator.onAccept();

        assertTrue(mModel.get(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE));
        verify(mBottomSheetController, times(0)).hideContent(any(), anyBoolean(), anyInt());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VCN_ENROLL_LOADING_AND_CONFIRMATION})
    public void testOnAccept_hidesBottomSheet() {
        mMediator.requestShowContent(mWindow);
        mMediator.onAccept();

        assertFalse(mModel.get(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE));
        verify(mBottomSheetController)
                .hideContent(
                        eq(mContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testOnCancel() {
        mMediator.requestShowContent(mWindow);
        mMediator.onCancel();

        verify(mBottomSheetController)
                .hideContent(
                        eq(mContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testHideBottomSheetAfterShowing() {
        mMediator.requestShowContent(mWindow);
        mMediator.hide();

        verify(mBottomSheetController)
                .hideContent(
                        /* content= */ eq(mContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testHideBottomSheetWithoutShowing() {
        mMediator.hide();

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VCN_ENROLL_LOADING_AND_CONFIRMATION})
    public void testMetrics_hideAfterOnAccept_RecordsMetric() {
        HistogramWatcher loadingShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillVcnEnrollBottomSheetMediator.LOADING_SHOWN_HISTOGRAM, true);
        HistogramWatcher loadingResultHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillVcnEnrollBottomSheetMediator.LOADING_RESULT_HISTOGRAM,
                        VirtualCardEnrollmentBubbleResult.ACCEPTED);

        mMediator.requestShowContent(mWindow);
        mMediator.onAccept();
        mMediator.hide();

        loadingShownHistogram.assertExpected();
        loadingResultHistogram.assertExpected();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VCN_ENROLL_LOADING_AND_CONFIRMATION})
    public void testMetrics_hideAfterOnAccept_NoRecords() {
        HistogramWatcher loadingShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                AutofillVcnEnrollBottomSheetMediator.LOADING_SHOWN_HISTOGRAM)
                        .build();
        HistogramWatcher loadingResultHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                AutofillVcnEnrollBottomSheetMediator.LOADING_RESULT_HISTOGRAM)
                        .build();

        mMediator.requestShowContent(mWindow);
        mMediator.onAccept();
        mMediator.hide();

        loadingShownHistogram.assertExpected();
        loadingResultHistogram.assertExpected();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VCN_ENROLL_LOADING_AND_CONFIRMATION})
    public void testMetrics_onCanceledAfterOnAccept_RecordsClosedLoadingResult() {
        HistogramWatcher loadingResultHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillVcnEnrollBottomSheetMediator.LOADING_RESULT_HISTOGRAM,
                        VirtualCardEnrollmentBubbleResult.CLOSED);

        mMediator.requestShowContent(mWindow);
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

        mMediator.requestShowContent(mWindow);
        mMediator.hide();

        loadingResultHistogram.assertExpected();
    }
}
