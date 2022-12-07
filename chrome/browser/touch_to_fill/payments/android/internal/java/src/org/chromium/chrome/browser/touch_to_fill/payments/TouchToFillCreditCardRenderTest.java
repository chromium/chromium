// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;
import static org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * These tests render screenshots of touch to fill for credit cards sheet and compare them
 * to a gold standard.
 */
@RunWith(ParameterizedRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchToFillCreditCardRenderTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    @Mock
    private TouchToFillCreditCardComponent.Delegate mDelegateMock;

    private static final CreditCard VISA = new CreditCard(/* guid= */ "",
            /* origin= */ "",
            /* isLocal= */ false, /* isCached= */ false, /* name= */ "MasterCard",
            /* number= */ "4111111111111111",
            /* obfuscatedNumber= */ "", /* month= */ "5", AutofillTestHelper.nextYear(),
            /* basicCardIssuerNetwork =*/"visa",
            /* issuerIconDrawableId= */ R.drawable.visa_card, /* billingAddressId= */ "",
            /* serverId= */ "", /* instrumentId= */ 0, /* cardLabel= */ "", /* nickname= */ "",
            /* cardArtUrl= */ null,
            /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.UNSPECIFIED,
            /* productDescription= */ "", /* cardNameForAutofillDisplay= */ "Visa",
            /* obfuscatedLastFourDigits= */ "• • • • 1111");
    private static final CreditCard MASTER_CARD = new CreditCard(/* guid= */ "",
            /* origin= */ "",
            /* isLocal= */ false, /* isCached= */ false, /* name= */ "MasterCard",
            /* number= */ "5555555555554444",
            /* obfuscatedNumber= */ "", /* month= */ "8", AutofillTestHelper.nextYear(),
            /* basicCardIssuerNetwork =*/"mastercard",
            /* issuerIconDrawableId= */ R.drawable.mc_card, /* billingAddressId= */ "",
            /* serverId= */ "", /* instrumentId= */ 0, /* cardLabel= */ "", /* nickname= */ "",
            /* cardArtUrl= */ null,
            /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.UNSPECIFIED,
            /* productDescription= */ "", /* cardNameForAutofillDisplay= */ "Mastercard",
            /* obfuscatedLastFourDigits= */ "• • • • 4444");

    private BottomSheetController mBottomSheetController;
    private TouchToFillCreditCardCoordinator mCoordinator;

    public TouchToFillCreditCardRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mBottomSheetController = mActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
        runOnUiThreadBlocking(() -> {
            mCoordinator = new TouchToFillCreditCardCoordinator();
            mCoordinator.initialize(
                    mActivityTestRule.getActivity(), mBottomSheetController, mDelegateMock);
        });
    }

    @After
    public void tearDown() {
        setRtlForTesting(false);
        try {
            finishActivity(mActivityTestRule.getActivity());
        } catch (Exception e) {
            // Activity was already closed (e.g. due to last test tearing down the suite).
        }
        runOnUiThreadBlocking(() -> tearDownNightModeAfterChromeActivityDestroyed());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsVisaAndMastercard() throws IOException {
        runOnUiThreadBlocking(() -> {
            mCoordinator.showSheet(new CreditCard[] {VISA, MASTER_CARD}, true);
        });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "touch_to_fill_credit_card_sheet");
    }
}
