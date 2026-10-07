// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.notNullValue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.autofill.AutofillEnableResurrectingPaymentsUsersTreatmentArm;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.ButtonCompat;

/** Integration tests for the Autofill Payments Churned Users bottom sheet module. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillPaymentsChurnedUsersBottomSheetModuleTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AutofillPaymentsChurnedUsersBottomSheetCoordinator.Delegate mDelegate;

    private Activity mActivity;
    private AutofillPaymentsChurnedUsersBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mCoordinator =
                new AutofillPaymentsChurnedUsersBottomSheetCoordinator(
                        mActivity,
                        mBottomSheetController,
                        AutofillEnableResurrectingPaymentsUsersTreatmentArm.CONVENIENCE,
                        mDelegate);
    }

    @Test
    public void testRequestShowContent() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mCoordinator.requestShowContent();

        verify(mBottomSheetController)
                .requestShowContent(
                        any(AutofillPaymentsChurnedUsersBottomSheetContent.class),
                        /* animate= */ eq(true));
    }

    @Test
    public void testRequestShowContent_whenControllerRejects() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(false);
        mCoordinator.requestShowContent();

        verify(mDelegate).onUiNotShown();
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillPaymentsChurnedUsersBottomSheetContent.class),
                        /* animate= */ eq(false),
                        eq(BottomSheetController.StateChangeReason.NONE));
        verify(mBottomSheetController)
                .removeObserver(any(AutofillPaymentsChurnedUsersBottomSheetMediator.class));
    }

    @Test
    public void testInitialModelValues_convenienceArm() {
        ImageView headerIconView =
                mCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_header_icon);
        assertThat(headerIconView.getDrawable(), notNullValue());
        assertThat(
                shadowOf(headerIconView.getDrawable()).getCreatedFromResId(),
                equalTo(R.drawable.autofill_payments_churned_users_convenience_illustration));
        assertThat(headerIconView.getVisibility(), equalTo(View.VISIBLE));

        TextView titleView =
                mCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_title);
        assertThat(
                titleView.getText().toString(),
                equalTo(
                        mActivity.getString(
                                R.string.autofill_churned_users_bubble_convenience_title)));
        TextView descriptionView =
                mCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_description);
        assertThat(
                descriptionView.getText().toString(),
                equalTo(
                        mActivity.getString(
                                R.string.autofill_churned_users_bubble_convenience_description)));
        ButtonCompat acceptButton =
                mCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_accept_button);
        assertThat(
                acceptButton.getText().toString(),
                equalTo(
                        mActivity.getString(
                                R.string.autofill_churned_users_bubble_accept_button_label)));
        ButtonCompat cancelButton =
                mCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_cancel_button);
        assertThat(
                cancelButton.getText().toString(),
                equalTo(
                        mActivity.getString(
                                R.string.autofill_churned_users_bubble_cancel_button_label)));
    }

    @Test
    public void testInitialModelValues_securityArm() {
        AutofillPaymentsChurnedUsersBottomSheetCoordinator securityCoordinator =
                new AutofillPaymentsChurnedUsersBottomSheetCoordinator(
                        mActivity,
                        mBottomSheetController,
                        AutofillEnableResurrectingPaymentsUsersTreatmentArm.SECURITY,
                        mDelegate);
        ImageView headerIconView =
                securityCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_header_icon);
        assertThat(headerIconView.getDrawable(), notNullValue());
        assertThat(
                shadowOf(headerIconView.getDrawable()).getCreatedFromResId(),
                equalTo(R.drawable.autofill_payments_churned_users_security_illustration));
        assertThat(headerIconView.getVisibility(), equalTo(View.VISIBLE));
        TextView titleView =
                securityCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_title);
        assertThat(
                titleView.getText().toString(),
                equalTo(
                        mActivity.getString(
                                R.string.autofill_churned_users_bubble_security_title)));
        TextView descriptionView =
                securityCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_description);
        assertThat(
                descriptionView.getText().toString(),
                equalTo(
                        mActivity.getString(
                                R.string.autofill_churned_users_bubble_security_description)));
        ButtonCompat acceptButton =
                securityCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_accept_button);
        assertThat(
                acceptButton.getText().toString(),
                equalTo(
                        mActivity.getString(
                                R.string.autofill_churned_users_bubble_accept_button_label)));
        ButtonCompat cancelButton =
                securityCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_cancel_button);
        assertThat(
                cancelButton.getText().toString(),
                equalTo(
                        mActivity.getString(
                                R.string.autofill_churned_users_bubble_cancel_button_label)));
        securityCoordinator.destroy();
    }

    @Test
    public void testAcceptButton_clicks_hidesContent() {
        ButtonCompat acceptButton =
                mCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_accept_button);
        acceptButton.performClick();

        verify(mDelegate).onUiAccepted();
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillPaymentsChurnedUsersBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testCancelButton_clicks_hidesContent() {
        ButtonCompat cancelButton =
                mCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_cancel_button);
        cancelButton.performClick();

        verify(mDelegate).onUiCanceled();
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillPaymentsChurnedUsersBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testDestroy() {
        mCoordinator.destroy();

        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillPaymentsChurnedUsersBottomSheetContent.class),
                        /* animate= */ eq(false),
                        eq(BottomSheetController.StateChangeReason.NONE));
        verify(mBottomSheetController)
                .removeObserver(any(AutofillPaymentsChurnedUsersBottomSheetMediator.class));
    }

    @Test
    public void testOnSheetClosed_notifiesDelegate() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mCoordinator.requestShowContent();

        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController).addObserver(observerCaptor.capture());

        observerCaptor.getValue().onSheetClosed(BottomSheetController.StateChangeReason.SWIPE);

        verify(mDelegate).onUiDismissed();
    }
}
