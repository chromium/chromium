// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;
import static org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.view.View;

import androidx.test.filters.MediumTest;

import com.google.android.material.progressindicator.CircularProgressIndicator;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.autofill.AutofillEnableResurrectingPaymentsUsersTreatmentArm;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * Render tests for {@link AutofillPaymentsChurnedUsersBottomSheetView}, checking the Payments
 * Churned Users bottom sheet against gold standards across treatment arms and loading state.
 */
@DoNotBatch(reason = "The tests can't be batched because they run for different set-ups.")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillPaymentsChurnedUsersBottomSheetRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    @Mock private AutofillPaymentsChurnedUsersBottomSheetCoordinator.Delegate mDelegate;

    private BottomSheetController mBottomSheetController;
    private AutofillPaymentsChurnedUsersBottomSheetCoordinator mCoordinator;

    public AutofillPaymentsChurnedUsersBottomSheetRenderTest(
            boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();
        mBottomSheetController =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
    }

    @After
    public void tearDown() {
        setRtlForTesting(false);
        if (mCoordinator != null) {
            runOnUiThreadBlocking(() -> mCoordinator.destroy());
        }
        try {
            finishActivity(mActivityTestRule.getActivity());
        } catch (Exception e) {
            // Activity was already closed.
        }
        runOnUiThreadBlocking(() -> tearDownNightModeAfterChromeActivityDestroyed());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsSecurityArm() throws IOException {
        showBottomSheet(AutofillEnableResurrectingPaymentsUsersTreatmentArm.SECURITY);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(
                bottomSheetView, "autofill_payments_churned_users_bottom_sheet_security");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsConvenienceArm() throws IOException {
        showBottomSheet(AutofillEnableResurrectingPaymentsUsersTreatmentArm.CONVENIENCE);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(
                bottomSheetView, "autofill_payments_churned_users_bottom_sheet_convenience");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsLoadingState() throws IOException {
        showBottomSheet(AutofillEnableResurrectingPaymentsUsersTreatmentArm.SECURITY);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator
                            .getModelForTesting()
                            .set(
                                    AutofillPaymentsChurnedUsersBottomSheetProperties
                                            .SHOW_LOADING_STATE,
                                    true);
                    CircularProgressIndicator spinner =
                            bottomSheetView.findViewById(
                                    R.id.payments_churned_users_loading_spinner);
                    spinner.setIndeterminate(false);
                    spinner.setProgress(50);
                });

        mRenderTestRule.render(
                bottomSheetView, "autofill_payments_churned_users_bottom_sheet_loading");
    }

    private void showBottomSheet(
            @AutofillEnableResurrectingPaymentsUsersTreatmentArm int treatmentArm) {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AutofillPaymentsChurnedUsersBottomSheetCoordinator(
                                    mActivityTestRule.getActivity(),
                                    mBottomSheetController,
                                    treatmentArm,
                                    mDelegate);
                    mCoordinator.requestShowContent();
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
    }
}
