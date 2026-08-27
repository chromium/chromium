// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.email_verification;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillSheetUiController;
import org.chromium.components.autofill.EmailVerificationPermissionUiStatus;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link EmailVerificationBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class EmailVerificationBottomSheetMediatorTest {
    private static final String TEST_TITLE = "Verify this email automatically?";
    private static final String TEST_DESCRIPTION =
            "While you're signed in, google.com can confirm test@example.com on supported sites so"
                    + " you don't have to check your inbox";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private EmailVerificationBottomSheetContent mContent;
    @Mock private AutofillSheetUiController mUiController;
    @Mock private EmailVerificationBottomSheetCoordinator.Delegate mDelegate;

    private Activity mActivity;
    private EmailVerificationBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
        mMediator =
                new EmailVerificationBottomSheetMediator(
                        mActivity,
                        TEST_TITLE,
                        TEST_DESCRIPTION,
                        mContent,
                        mUiController,
                        mDelegate);
    }

    @Test
    public void testModelInitialization() {
        PropertyModel model = mMediator.getModel();
        assertNotNull(model);
        assertEquals(TEST_TITLE, model.get(EmailVerificationBottomSheetProperties.TITLE));
        assertEquals(
                TEST_DESCRIPTION, model.get(EmailVerificationBottomSheetProperties.DESCRIPTION));
        assertEquals(
                mActivity.getString(R.string.autofill_email_verifier_prompt_verify),
                model.get(EmailVerificationBottomSheetProperties.CONFIRM_BUTTON_LABEL));
        assertEquals(
                mActivity.getString(R.string.autofill_email_verifier_prompt_not_now),
                model.get(EmailVerificationBottomSheetProperties.CANCEL_BUTTON_LABEL));
        assertTrue(model.get(EmailVerificationBottomSheetProperties.DRAG_HANDLE_VISIBLE));
    }

    @Test
    public void testModelConfirmCallback_triggersOnAccepted() {
        PropertyModel model = mMediator.getModel();
        Runnable confirmCallback =
                model.get(EmailVerificationBottomSheetProperties.ON_CONFIRM_CLICKED);
        assertNotNull(confirmCallback);
        confirmCallback.run();

        verify(mDelegate).onUiDecision(EmailVerificationPermissionUiStatus.ALLOWED);
    }

    @Test
    public void testModelCancelCallback_triggersOnDeclined() {
        PropertyModel model = mMediator.getModel();
        Runnable cancelCallback =
                model.get(EmailVerificationBottomSheetProperties.ON_CANCEL_CLICKED);
        assertNotNull(cancelCallback);
        cancelCallback.run();

        verify(mDelegate).onUiDecision(EmailVerificationPermissionUiStatus.DECLINED);
    }

    @Test
    public void testRequestShowContent_success() {
        when(mUiController.requestShowContent(eq(mContent), eq(true))).thenReturn(true);

        mMediator.requestShowContent();

        verify(mUiController).addObserver(mMediator);
        verify(mDelegate).onUiShown();
    }

    @Test
    public void testRequestShowContent_failure() {
        when(mUiController.requestShowContent(eq(mContent), eq(true))).thenReturn(false);

        mMediator.requestShowContent();

        verify(mDelegate).onUiDecision(EmailVerificationPermissionUiStatus.OTHER);
    }

    @Test
    public void testOnAccepted() {
        mMediator.onAccepted();

        verify(mUiController).removeObserver(mMediator);
        verify(mUiController)
                .hideContent(
                        eq(mContent),
                        /* animate= */ eq(true),
                        eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mDelegate).onUiDecision(EmailVerificationPermissionUiStatus.ALLOWED);
    }

    @Test
    public void testOnDeclined() {
        mMediator.onDeclined();

        verify(mUiController).removeObserver(mMediator);
        verify(mUiController)
                .hideContent(
                        eq(mContent),
                        /* animate= */ eq(true),
                        eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mDelegate).onUiDecision(EmailVerificationPermissionUiStatus.DECLINED);
    }

    @Test
    public void testOnSheetClosed_userAborted() {
        mMediator.onSheetClosed(StateChangeReason.BACK_PRESS);

        verify(mDelegate).onUiDecision(EmailVerificationPermissionUiStatus.USER_ABORTED);
    }

    @Test
    public void testOnSheetClosed_tabGone() {
        mMediator.onSheetClosed(StateChangeReason.NAVIGATION);

        verify(mDelegate).onUiDecision(EmailVerificationPermissionUiStatus.TAB_GONE);
    }

    @Test
    public void testOnSheetClosed_other() {
        mMediator.onSheetClosed(StateChangeReason.NONE);

        verify(mDelegate).onUiDecision(EmailVerificationPermissionUiStatus.OTHER);
    }

    @Test
    public void testOnSheetClosed_interactionComplete() {
        mMediator.onSheetClosed(StateChangeReason.INTERACTION_COMPLETE);

        verify(mDelegate, never()).onUiDecision(anyInt());
    }

    @Test
    public void testRapidClicksGuard() {
        mMediator.onAccepted();
        mMediator.onDeclined();
        mMediator.onAccepted();

        verify(mDelegate, times(1)).onUiDecision(EmailVerificationPermissionUiStatus.ALLOWED);
        verify(mDelegate, times(0)).onUiDecision(EmailVerificationPermissionUiStatus.DECLINED);
    }

    @Test
    public void testHide() {
        mMediator.hide(StateChangeReason.BACK_PRESS);

        verify(mUiController).removeObserver(mMediator);
        verify(mUiController)
                .hideContent(
                        eq(mContent), /* animate= */ eq(true), eq(StateChangeReason.BACK_PRESS));
    }

    private static int anyInt() {
        return org.mockito.ArgumentMatchers.anyInt();
    }
}
