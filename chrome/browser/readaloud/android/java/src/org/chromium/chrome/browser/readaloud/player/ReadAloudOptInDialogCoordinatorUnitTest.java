// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ReadAloudOptInDialogCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReadAloudOptInDialogCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Callback<Boolean> mOnOptInDecision;
    @Captor private ArgumentCaptor<PropertyModel> mModelCaptor;

    private Activity mActivity;
    private ReadAloudOptInDialogCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new ReadAloudOptInDialogCoordinator(
                        mActivity, mModalDialogManager, mOnOptInDecision);
    }

    @Test
    public void testShowPopulatesDialogViewsAndButtons() {
        mCoordinator.show();

        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mModelCaptor.getValue();

        assertEquals(
                mActivity.getString(R.string.readaloud_opt_in_accept_button),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                mActivity.getString(R.string.cancel),
                model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        assertFalse(model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));

        View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
        assertNotNull(customView);
        // Content must scroll so the disclaimer is reachable with large fonts or in landscape.
        assertTrue(customView instanceof ScrollView);
        TextView titleView = customView.findViewById(R.id.readaloud_opt_in_title);
        TextView messageView = customView.findViewById(R.id.readaloud_opt_in_message);
        assertEquals(
                mActivity.getString(R.string.readaloud_opt_in_dialog_title),
                titleView.getText().toString());
        assertEquals(
                mActivity.getString(R.string.readaloud_opt_in_dialog_message),
                messageView.getText().toString());
    }

    @Test
    public void testAcceptCallsOnOptInDecisionTrue() {
        mCoordinator.show();
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mModelCaptor.getValue();

        model.get(ModalDialogProperties.CONTROLLER)
                .onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mModalDialogManager)
                .dismissDialog(eq(model), eq(DialogDismissalCause.POSITIVE_BUTTON_CLICKED));

        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        verify(mOnOptInDecision).onResult(eq(true));
    }

    @Test
    public void testDeclineCallsOnOptInDecisionFalse() {
        mCoordinator.show();
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mModelCaptor.getValue();

        model.get(ModalDialogProperties.CONTROLLER)
                .onClick(model, ModalDialogProperties.ButtonType.NEGATIVE);
        verify(mModalDialogManager)
                .dismissDialog(eq(model), eq(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED));

        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        verify(mOnOptInDecision).onResult(eq(false));
    }

    @Test
    public void testShowWhileShowingIsNoOp() {
        mCoordinator.show();
        mCoordinator.show();
        verify(mModalDialogManager, times(1)).showDialog(any(), eq(ModalDialogType.APP));
    }

    @Test
    public void testDismissDoesNotCallOnOptInDecision() {
        mCoordinator.show();
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mModelCaptor.getValue();

        mCoordinator.dismiss();
        verify(mModalDialogManager)
                .dismissDialog(eq(model), eq(DialogDismissalCause.ACTIVITY_DESTROYED));

        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.ACTIVITY_DESTROYED);
        verify(mOnOptInDecision, never()).onResult(any());
    }

    @Test
    public void testBackPressCallsOnOptInDecisionFalse() {
        mCoordinator.show();
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mModelCaptor.getValue();

        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        verify(mOnOptInDecision).onResult(eq(false));
    }

    @Test
    public void testNavigateBackCallsOnOptInDecisionFalse() {
        mCoordinator.show();
        verify(mModalDialogManager).showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP));
        PropertyModel model = mModelCaptor.getValue();

        model.get(ModalDialogProperties.CONTROLLER)
                .onDismiss(model, DialogDismissalCause.NAVIGATE_BACK);
        verify(mOnOptInDecision).onResult(eq(false));
    }
}
