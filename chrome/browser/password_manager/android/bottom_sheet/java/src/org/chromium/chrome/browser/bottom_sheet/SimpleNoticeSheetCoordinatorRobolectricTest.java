// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottom_sheet;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.text.SpannableString;
import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.CallbackUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link TouchToFillPasswordGenerationBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SimpleNoticeSheetCoordinatorRobolectricTest {
    private SimpleNoticeSheetCoordinator mCoordinator;
    private final ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor =
            ArgumentCaptor.forClass(BottomSheetObserver.class);
    private static final String sTitle = "Simple notice sheet title";
    private static final SpannableString sText =
            SpannableString.valueOf("The text about the simple notice sheet");
    private static final String sButtonText = "Button";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Mock private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        mCoordinator =
                new SimpleNoticeSheetCoordinator(
                        ContextUtils.getApplicationContext(), mBottomSheetController);
    }

    private void setUpBottomSheetController() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        doNothing().when(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
    }

    @Test
    public void showsAndHidesBottomSheet() {
        setUpBottomSheetController();

        PropertyModel model =
                new PropertyModel.Builder(SimpleNoticeSheetProperties.ALL_KEYS)
                        .with(SimpleNoticeSheetProperties.SHEET_TITLE, sTitle)
                        .with(SimpleNoticeSheetProperties.SHEET_TEXT, sText)
                        .with(SimpleNoticeSheetProperties.BUTTON_TITLE, sButtonText)
                        .with(
                                SimpleNoticeSheetProperties.BUTTON_ACTION,
                                CallbackUtils.emptyRunnable())
                        .build();
        mCoordinator.showSheet(model);
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        verify(mBottomSheetController).addObserver(any());

        mCoordinator.onDismissed(StateChangeReason.SWIPE);
        verify(mBottomSheetController).removeObserver(mBottomSheetObserverCaptor.getValue());
    }

    @Test
    public void testConsumesGenericMotionEventsToPreventMouseClicksThroughSheet() {
        ArgumentCaptor<BottomSheetContent> contentCaptor =
                ArgumentCaptor.forClass(BottomSheetContent.class);

        when(mBottomSheetController.requestShowContent(contentCaptor.capture(), anyBoolean()))
                .thenReturn(true);

        mCoordinator.showSheet(
                new PropertyModel.Builder(SimpleNoticeSheetProperties.ALL_KEYS)
                        .with(SimpleNoticeSheetProperties.SHEET_TITLE, sTitle)
                        .with(SimpleNoticeSheetProperties.SHEET_TEXT, sText)
                        .with(SimpleNoticeSheetProperties.BUTTON_TITLE, sButtonText)
                        .with(SimpleNoticeSheetProperties.BUTTON_ACTION, () -> {})
                        .build());

        assertTrue(
                contentCaptor
                        .getValue()
                        .getContentView()
                        .dispatchGenericMotionEvent(mock(MotionEvent.class)));
    }
}
