// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.email_verification;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.widget.Button;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.anchored_dialog.AnchoredDialogCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.autofill.EmailVerificationPermissionUiStatus;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link EmailVerificationBottomSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class EmailVerificationBottomSheetCoordinatorTest {
    private static final String TEST_TITLE = "Verify this email automatically?";
    private static final String TEST_DESCRIPTION =
            "While you're signed in, google.com can confirm test@example.com on supported sites so"
                    + " you don't have to check your inbox";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private EmailVerificationBottomSheetCoordinator.Delegate mDelegate;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AnchoredDialogCoordinator mAnchoredDialogCoordinator;

    private Activity mActivity;
    private EmailVerificationBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mActivity = Robolectric.setupActivity(TestActivity.class);
        mCoordinator =
                new EmailVerificationBottomSheetCoordinator(
                        mActivity,
                        TEST_TITLE,
                        TEST_DESCRIPTION,
                        mBottomSheetController,
                        mAnchoredDialogCoordinator,
                        mDelegate);
    }

    @Test
    public void testRequestShowContent() {
        mCoordinator.requestShowContent();

        verify(mBottomSheetController)
                .requestShowContent(
                        any(EmailVerificationBottomSheetContent.class), /* animate= */ eq(true));
        verify(mDelegate).onUiShown();
    }

    @Test
    public void testHide() {
        mCoordinator.requestShowContent();
        mCoordinator.hide(StateChangeReason.NONE);

        verify(mBottomSheetController)
                .hideContent(
                        any(EmailVerificationBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(StateChangeReason.NONE));
    }

    @Test
    public void testModelBinding() {
        PropertyModel model = mCoordinator.getPropertyModelForTesting();
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

        EmailVerificationBottomSheetView view = mCoordinator.getViewForTesting();
        TextView titleView = view.mContentView.findViewById(R.id.email_verification_title_text);
        assertEquals(TEST_TITLE, titleView.getText().toString());

        TextView descView =
                view.mContentView.findViewById(R.id.email_verification_description_text);
        assertEquals(TEST_DESCRIPTION, descView.getText().toString());

        Button confirmBtn = view.mContentView.findViewById(R.id.email_verification_confirm_button);
        assertEquals(
                mActivity.getString(R.string.autofill_email_verifier_prompt_verify),
                confirmBtn.getText().toString());

        Button cancelBtn = view.mContentView.findViewById(R.id.email_verification_cancel_button);
        assertEquals(
                mActivity.getString(R.string.autofill_email_verifier_prompt_not_now),
                cancelBtn.getText().toString());
    }

    @Test
    public void testConfirmButtonClicked() {
        EmailVerificationBottomSheetView view = mCoordinator.getViewForTesting();
        Button confirmBtn = view.mContentView.findViewById(R.id.email_verification_confirm_button);
        confirmBtn.performClick();

        verify(mDelegate).onUiDecision(EmailVerificationPermissionUiStatus.ALLOWED);
    }

    @Test
    public void testCancelButtonClicked() {
        EmailVerificationBottomSheetView view = mCoordinator.getViewForTesting();
        Button cancelBtn = view.mContentView.findViewById(R.id.email_verification_cancel_button);
        cancelBtn.performClick();

        verify(mDelegate).onUiDecision(EmailVerificationPermissionUiStatus.DECLINED);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures({ChromeFeatureList.ANDROID_SAVE_CARD_NON_BLOCKING_DIALOG})
    public void testRequestShowContent_onTablet_usesAnchoredDialog() {
        when(mAnchoredDialogCoordinator.requestShowContent(any())).thenReturn(true);
        Activity tabletActivity = Robolectric.setupActivity(TestActivity.class);
        EmailVerificationBottomSheetCoordinator tabletCoordinator =
                new EmailVerificationBottomSheetCoordinator(
                        tabletActivity,
                        TEST_TITLE,
                        TEST_DESCRIPTION,
                        mBottomSheetController,
                        mAnchoredDialogCoordinator,
                        mDelegate);

        tabletCoordinator.requestShowContent();

        verify(mAnchoredDialogCoordinator)
                .requestShowContent(any(EmailVerificationBottomSheetContent.class));
        verify(mDelegate).onUiShown();
    }
}
