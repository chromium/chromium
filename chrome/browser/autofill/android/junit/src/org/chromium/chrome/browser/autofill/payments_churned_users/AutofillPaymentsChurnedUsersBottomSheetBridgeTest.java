// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.Matchers.nullValue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.autofill.AutofillEnableResurrectingPaymentsUsersTreatmentArm;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link AutofillPaymentsChurnedUsersBottomSheetBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillPaymentsChurnedUsersBottomSheetBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private WindowAndroid mWindow;
    @Mock private ManagedBottomSheetController mBottomSheetController;
    private AutofillPaymentsChurnedUsersBottomSheetBridge mBridge;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mWindow = new WindowAndroid(mActivity, /* occlusionTrackingAllowed= */ true);
        lenient()
                .when(mBottomSheetController.requestShowContent(any(), anyBoolean()))
                .thenReturn(true);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        mBridge = new AutofillPaymentsChurnedUsersBottomSheetBridge(mWindow);
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    public void testRequestShowContent() {
        mBridge.requestShowContent(AutofillEnableResurrectingPaymentsUsersTreatmentArm.CONVENIENCE);

        verify(mBottomSheetController)
                .requestShowContent(
                        any(AutofillPaymentsChurnedUsersBottomSheetContent.class),
                        /* animate= */ eq(true));
        assertThat(mBridge.getCoordinatorForTesting(), notNullValue());
    }

    @Test
    public void testRequestShowContent_calledMultipleTimes_destroysPreviousCoordinator() {
        mBridge.requestShowContent(AutofillEnableResurrectingPaymentsUsersTreatmentArm.SECURITY);
        AutofillPaymentsChurnedUsersBottomSheetCoordinator firstCoordinator =
                mBridge.getCoordinatorForTesting();
        assertThat(firstCoordinator, notNullValue());

        mBridge.requestShowContent(AutofillEnableResurrectingPaymentsUsersTreatmentArm.CONVENIENCE);
        AutofillPaymentsChurnedUsersBottomSheetCoordinator secondCoordinator =
                mBridge.getCoordinatorForTesting();
        assertThat(secondCoordinator, notNullValue());

        // Verify hideContent was called for destroying the first coordinator
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillPaymentsChurnedUsersBottomSheetContent.class),
                        /* animate= */ eq(false),
                        eq(BottomSheetController.StateChangeReason.NONE));
    }

    @Test
    public void testRequestShowContent_whenBottomSheetControllerNull() {
        WindowAndroid windowWithoutController =
                new WindowAndroid(mActivity, /* occlusionTrackingAllowed= */ true);
        AutofillPaymentsChurnedUsersBottomSheetBridge bridge =
                new AutofillPaymentsChurnedUsersBottomSheetBridge(windowWithoutController);

        bridge.requestShowContent(AutofillEnableResurrectingPaymentsUsersTreatmentArm.CONVENIENCE);

        assertThat(bridge.getCoordinatorForTesting(), nullValue());
        windowWithoutController.destroy();
    }

    @Test
    public void testRequestShowContent_whenContextNull() {
        WindowAndroid window = new WindowAndroid(mActivity, /* occlusionTrackingAllowed= */ true);
        BottomSheetControllerFactory.attach(window, mBottomSheetController);
        AutofillPaymentsChurnedUsersBottomSheetBridge bridge =
                new AutofillPaymentsChurnedUsersBottomSheetBridge(window);
        window.destroy();

        bridge.requestShowContent(AutofillEnableResurrectingPaymentsUsersTreatmentArm.CONVENIENCE);

        assertThat(bridge.getCoordinatorForTesting(), nullValue());
    }

    @Test
    public void testDestroy() {
        mBridge.requestShowContent(AutofillEnableResurrectingPaymentsUsersTreatmentArm.CONVENIENCE);
        assertThat(mBridge.getCoordinatorForTesting(), notNullValue());

        mBridge.destroy();

        assertThat(mBridge.getCoordinatorForTesting(), nullValue());
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillPaymentsChurnedUsersBottomSheetContent.class),
                        /* animate= */ eq(false),
                        eq(BottomSheetController.StateChangeReason.NONE));
        verify(mBottomSheetController)
                .removeObserver(any(AutofillPaymentsChurnedUsersBottomSheetMediator.class));
    }

    @Test
    public void testDestroy_whenCoordinatorNotCreated() {
        mBridge.destroy();

        verifyNoInteractions(mBottomSheetController);
    }
}
