// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.autofill.AutofillEnableResurrectingPaymentsUsersTreatmentArm;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.TestActivity;

/** Integration tests for the Autofill Payments Churned Users bottom sheet module. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillPaymentsChurnedUsersBottomSheetModuleTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BottomSheetController mBottomSheetController;

    private Activity mActivity;
    private AutofillPaymentsChurnedUsersBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mCoordinator =
                new AutofillPaymentsChurnedUsersBottomSheetCoordinator(
                        mActivity,
                        mBottomSheetController,
                        AutofillEnableResurrectingPaymentsUsersTreatmentArm.CONVENIENCE);
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
        TextView titleView =
                mCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_title);
        assertThat(
                titleView.getText().toString(),
                equalTo(
                        mActivity.getString(
                                R.string.autofill_churned_users_bubble_convenience_title)));
    }

    @Test
    public void testInitialModelValues_securityArm() {
        AutofillPaymentsChurnedUsersBottomSheetCoordinator securityCoordinator =
                new AutofillPaymentsChurnedUsersBottomSheetCoordinator(
                        mActivity,
                        mBottomSheetController,
                        AutofillEnableResurrectingPaymentsUsersTreatmentArm.SECURITY);
        TextView titleView =
                securityCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.payments_churned_users_title);
        assertThat(
                titleView.getText().toString(),
                equalTo(
                        mActivity.getString(
                                R.string.autofill_churned_users_bubble_security_title)));
        securityCoordinator.destroy();
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
}
