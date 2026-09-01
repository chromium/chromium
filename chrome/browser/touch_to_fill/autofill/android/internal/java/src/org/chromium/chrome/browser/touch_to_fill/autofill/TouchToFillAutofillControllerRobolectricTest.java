// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.payments.R;
import org.chromium.components.autofill.PopupNoticeInteractions;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Tests for {@link TouchToFillAutofillCoordinator} and {@link TouchToFillAutofillMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TouchToFillAutofillControllerRobolectricTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TouchToFillAutofillComponent.Delegate mDelegateMock;
    @Mock private BottomSheetFocusHelper mBottomSheetFocusHelper;
    @Captor private ArgumentCaptor<BottomSheetContent> mContentCaptor;

    private Activity mActivity;
    private TouchToFillAutofillCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        when(mBottomSheetController.requestShowContent(any(BottomSheetContent.class), anyBoolean()))
                .thenReturn(true);

        mCoordinator =
                new TouchToFillAutofillCoordinator(
                        mActivity, mBottomSheetController, mDelegateMock, mBottomSheetFocusHelper);
    }

    @Test
    public void testShowPersonalContextNotice() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TouchToFillAutofillMediator.NOTICE_INTERACTIONS_HISTOGRAM,
                        PopupNoticeInteractions.SHOWN);

        mCoordinator.show();

        histogramWatcher.assertExpected();
        verify(mBottomSheetFocusHelper).registerForOneTimeUse();
        verify(mBottomSheetController).requestShowContent(any(BottomSheetContent.class), eq(true));
    }

    @Test
    public void testAcknowledgeNotice() {
        HistogramWatcher shownWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TouchToFillAutofillMediator.NOTICE_INTERACTIONS_HISTOGRAM,
                        PopupNoticeInteractions.SHOWN);
        mCoordinator.show();
        shownWatcher.assertExpected();

        verify(mBottomSheetController).requestShowContent(mContentCaptor.capture(), eq(true));

        View contentView = mContentCaptor.getValue().getContentView();
        assertNotNull(contentView);
        View okButton = contentView.findViewById(R.id.notice_acknowledge_button);
        assertNotNull(okButton);

        HistogramWatcher ackWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TouchToFillAutofillMediator.NOTICE_INTERACTIONS_HISTOGRAM,
                        PopupNoticeInteractions.ACKNOWLEDGED);
        okButton.performClick();
        ackWatcher.assertExpected();

        verify(mDelegateMock).onNoticeAcknowledged();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), eq(true));
    }

    @Test
    public void testSettingsLink() {
        HistogramWatcher shownWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TouchToFillAutofillMediator.NOTICE_INTERACTIONS_HISTOGRAM,
                        PopupNoticeInteractions.SHOWN);
        mCoordinator.show();
        shownWatcher.assertExpected();

        verify(mBottomSheetController).requestShowContent(mContentCaptor.capture(), eq(true));

        View contentView = mContentCaptor.getValue().getContentView();
        assertNotNull(contentView);
        View settingsLink = contentView.findViewById(R.id.notice_manage_settings_link);
        assertNotNull(settingsLink);

        HistogramWatcher settingsWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TouchToFillAutofillMediator.NOTICE_INTERACTIONS_HISTOGRAM,
                        PopupNoticeInteractions.LINK_BUTTON_CLICKED);
        settingsLink.performClick();
        settingsWatcher.assertExpected();

        verify(mDelegateMock).onSettingsLinkClicked();
        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), eq(true));
    }

    @Test
    public void testHideSheet() {
        HistogramWatcher shownWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TouchToFillAutofillMediator.NOTICE_INTERACTIONS_HISTOGRAM,
                        PopupNoticeInteractions.SHOWN);
        mCoordinator.show();
        shownWatcher.assertExpected();

        HistogramWatcher dismissedWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        TouchToFillAutofillMediator.NOTICE_INTERACTIONS_HISTOGRAM,
                        PopupNoticeInteractions.DISMISSED);
        mCoordinator.hide();
        dismissedWatcher.assertExpected();

        verify(mBottomSheetController).hideContent(any(BottomSheetContent.class), eq(true));
        verify(mDelegateMock).onDismissed();
    }
}
