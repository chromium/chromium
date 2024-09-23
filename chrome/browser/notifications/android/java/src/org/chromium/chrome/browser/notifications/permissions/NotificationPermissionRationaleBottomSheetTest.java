// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.NotificationRationaleResult;
import org.chromium.chrome.browser.notifications.R;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController.RationaleUiResult;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/** Tests for {@link NotificationPermissionRationaleBottomSheet}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NotificationPermissionRationaleBottomSheetTest {
    private BottomSheetController mBottomSheetController;
    private Context mContext;

    @Captor ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    @Mock Callback<Integer> mMockCallback;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mBottomSheetController = Mockito.mock(BottomSheetController.class);
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);

        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testShowBottomSheet() {
        NotificationPermissionRationaleBottomSheet bottomSheet =
                new NotificationPermissionRationaleBottomSheet(mContext, mBottomSheetController);

        // Show the bottom sheet, we don't dismiss it so the callback shouldn't be called.
        bottomSheet.showRationaleUi(mMockCallback);

        verify(mBottomSheetController).requestShowContent(bottomSheet, true);
        verify(mBottomSheetController).addObserver(any());

        View bottomSheetContentView = bottomSheet.getContentView();

        assertNotNull(bottomSheetContentView);

        View bottomSheetTitle =
                bottomSheetContentView.findViewById(R.id.notification_permission_rationale_title);
        View bottomSheetMessage =
                bottomSheetContentView.findViewById(R.id.notification_permission_rationale_message);

        verify(mMockCallback, never()).onResult(anyInt());
        // Check that the custom view contains the expected title and message.
        assertThat(
                bottomSheetTitle,
                withText(R.string.notification_permission_rationale_dialog_title));
        assertThat(
                bottomSheetMessage,
                withText(R.string.notification_permission_rationale_dialog_message));
    }

    @Test
    public void testRejectBottomSheet() {
        NotificationPermissionRationaleBottomSheet bottomSheet =
                new NotificationPermissionRationaleBottomSheet(mContext, mBottomSheetController);

        bottomSheet.showRationaleUi(mMockCallback);

        View bottomSheetContentView = bottomSheet.getContentView();
        View bottomSheetNegativeButton =
                bottomSheetContentView.findViewById(
                        R.id.notification_permission_rationale_negative_button);
        bottomSheetNegativeButton.performClick();

        verify(mMockCallback).onResult(RationaleUiResult.REJECTED);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.RationaleResult",
                        NotificationRationaleResult.NEGATIVE_BUTTON_CLICKED));
    }

    @Test
    public void testDismissBottomSheet_BackPress() {
        NotificationPermissionRationaleBottomSheet bottomSheet =
                new NotificationPermissionRationaleBottomSheet(mContext, mBottomSheetController);

        bottomSheet.showRationaleUi(mMockCallback);

        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());

        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.BACK_PRESS);

        verify(mMockCallback).onResult(RationaleUiResult.REJECTED);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.RationaleResult",
                        NotificationRationaleResult.BOTTOM_SHEET_BACK_PRESS));
    }

    @Test
    public void testDismissBottomSheet_SwipeAway() {
        NotificationPermissionRationaleBottomSheet bottomSheet =
                new NotificationPermissionRationaleBottomSheet(mContext, mBottomSheetController);

        bottomSheet.showRationaleUi(mMockCallback);

        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());

        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.SWIPE);

        verify(mMockCallback).onResult(RationaleUiResult.REJECTED);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.RationaleResult",
                        NotificationRationaleResult.BOTTOM_SHEET_SWIPE));
    }

    @Test
    public void testDismissBottomSheet_TapOutside() {
        NotificationPermissionRationaleBottomSheet bottomSheet =
                new NotificationPermissionRationaleBottomSheet(mContext, mBottomSheetController);

        bottomSheet.showRationaleUi(mMockCallback);

        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());

        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.TAP_SCRIM);

        verify(mMockCallback).onResult(RationaleUiResult.REJECTED);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.RationaleResult",
                        NotificationRationaleResult.BOTTOM_SHEET_TAP_SCRIM));
    }

    @Test
    public void testAcceptBottomSheet() {
        NotificationPermissionRationaleBottomSheet bottomSheet =
                new NotificationPermissionRationaleBottomSheet(mContext, mBottomSheetController);

        bottomSheet.showRationaleUi(mMockCallback);

        View bottomSheetContentView = bottomSheet.getContentView();
        View bottomSheetPositiveButton =
                bottomSheetContentView.findViewById(
                        R.id.notification_permission_rationale_positive_button);
        bottomSheetPositiveButton.performClick();

        verify(mMockCallback).onResult(RationaleUiResult.ACCEPTED);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.RationaleResult",
                        NotificationRationaleResult.POSITIVE_BUTTON_CLICKED));
    }

    @Test
    public void testResultBottomSheetDestroyed_AfterOpened() {
        NotificationPermissionRationaleBottomSheet bottomSheet =
                new NotificationPermissionRationaleBottomSheet(mContext, mBottomSheetController);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(bottomSheet);

        bottomSheet.showRationaleUi(mMockCallback);

        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        mBottomSheetObserverCaptor.getValue().onSheetOpened(StateChangeReason.NONE);

        bottomSheet.destroy();

        verify(mMockCallback).onResult(RationaleUiResult.REJECTED);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.RationaleResult",
                        NotificationRationaleResult.BOTTOM_SHEET_DESTROYED));
    }

    @Test
    public void testResultBottomSheetDestroyed_WithoutOpening() {
        NotificationPermissionRationaleBottomSheet bottomSheet =
                new NotificationPermissionRationaleBottomSheet(mContext, mBottomSheetController);

        bottomSheet.showRationaleUi(mMockCallback);

        bottomSheet.destroy();

        verify(mMockCallback).onResult(RationaleUiResult.NOT_SHOWN);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.RationaleResult",
                        NotificationRationaleResult.BOTTOM_SHEET_NEVER_OPENED));
    }
}
