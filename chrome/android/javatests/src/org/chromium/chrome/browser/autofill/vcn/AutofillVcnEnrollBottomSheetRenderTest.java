// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.view.ViewGroup;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.LargeTest;

import com.google.common.collect.ImmutableList;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.Description;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.IssuerIcon;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.LegalMessages;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.LinkOpener;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.List;

@LargeTest
@Batch(Batch.PER_CLASS)
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({AutofillFeatures.AUTOFILL_ENABLE_VIRTUAL_CARD_JAVA_PAYMENTS_DATA_MANAGER})
public class AutofillVcnEnrollBottomSheetRenderTest {
    /**
     * A function that does nothing and conforms to {@link
     * org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.LinkOpener}'s
     * functional interface.
     */
    private static final LinkOpener DO_NOTHING_LINK_OPENER = (unusedUrl, unusedLinkType) -> {};

    @Rule
    public final FreshCtaTransitTestRule mTabbedActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    private BottomSheetController mBottomSheetController;
    private AutofillVcnEnrollBottomSheetContent mVcnEnrollBottomSheetContent;

    private final boolean mIsRightToLeftLayout;
    private final boolean mIsNightMode;

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            ImmutableList.of(
                    new ParameterSet().value(false, false).name("LTR"),
                    new ParameterSet().value(true, false).name("RTL"),
                    new ParameterSet().value(false, true).name("NightMode"));

    public AutofillVcnEnrollBottomSheetRenderTest(
            boolean isRightToLeftLayout, boolean isNightMode) {
        mIsRightToLeftLayout = isRightToLeftLayout;
        mIsNightMode = isNightMode;
    }

    @Before
    public void setUp() {
        setRtlForTesting(mIsRightToLeftLayout);
        mRenderTestRule.setVariantPrefix((mIsRightToLeftLayout ? "RTL" : "LTR"));
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(mIsNightMode);
        mRenderTestRule.setNightModeEnabled(mIsNightMode);
        mTabbedActivityTestRule.startOnBlankPage();
        mTabbedActivityTestRule.waitForActivityCompletelyLoaded();
        mBottomSheetController =
                mTabbedActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
    }

    @After
    public void tearDown() {
        runOnUiThreadBlocking(
                () -> {
                    if (mBottomSheetController.getCurrentSheetContent() != null) {
                        mBottomSheetController.hideContent(
                                mVcnEnrollBottomSheetContent, /* animate= */ false);
                    }
                });
        setRtlForTesting(false);
    }

    @Test
    @Feature({"RenderTest"})
    public void testVcnEnrollmentBottomSheetWithLongCardName() throws Exception {
        AutofillVcnEnrollBottomSheetView view =
                new AutofillVcnEnrollBottomSheetView(mTabbedActivityTestRule.getActivity());
        PropertyModel model =
                new PropertyModel.Builder(AutofillVcnEnrollBottomSheetProperties.ALL_KEYS)
                        .with(AutofillVcnEnrollBottomSheetProperties.ACCEPT_BUTTON_LABEL, "Yes")
                        .with(AutofillVcnEnrollBottomSheetProperties.CANCEL_BUTTON_LABEL, "Skip")
                        .with(
                                AutofillVcnEnrollBottomSheetProperties.CARD_LABEL,
                                "A Card Label That is Actually Quite Long ···· 1234")
                        .with(
                                AutofillVcnEnrollBottomSheetProperties.DESCRIPTION,
                                new Description(
                                        /* text= */ "Using virtual card numbers hides your actual"
                                                + " card to protect you from potential"
                                                + " fraud. Learn more",
                                        /* learnMoreLinkText= */ "Learn more",
                                        /* learnMoreLinkUrl= */ "https://example.test",
                                        VirtualCardEnrollmentLinkType
                                                .VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
                                        DO_NOTHING_LINK_OPENER))
                        .with(
                                AutofillVcnEnrollBottomSheetProperties.GOOGLE_LEGAL_MESSAGES,
                                new LegalMessages(
                                        ImmutableList.of(
                                                new LegalMessageLine(
                                                        /* text= */ "It's a message from the"
                                                                + " payments server about"
                                                                + " the virtual card"
                                                                + " enrollment.",
                                                        /* links= */ ImmutableList.of())),
                                        VirtualCardEnrollmentLinkType
                                                .VIRTUAL_CARD_ENROLLMENT_GOOGLE_PAYMENTS_TOS_LINK,
                                        DO_NOTHING_LINK_OPENER))
                        .with(
                                AutofillVcnEnrollBottomSheetProperties.ISSUER_ICON_FETCH_CALLBACK,
                                (issuerIcon) ->
                                        AppCompatResources.getDrawable(
                                                mTabbedActivityTestRule.getActivity(),
                                                issuerIcon.mIconResource))
                        .with(
                                AutofillVcnEnrollBottomSheetProperties.ISSUER_ICON,
                                new IssuerIcon(R.drawable.visa_card, /* iconUrl= */ null))
                        .with(
                                AutofillVcnEnrollBottomSheetProperties.ISSUER_LEGAL_MESSAGES,
                                new LegalMessages(
                                        ImmutableList.of(
                                                new LegalMessageLine(
                                                        /* text= */ "It's a message from the issuer"
                                                                + " about the virtual card"
                                                                + " enrollment.",
                                                        /* links= */ ImmutableList.of())),
                                        VirtualCardEnrollmentLinkType
                                                .VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK,
                                        DO_NOTHING_LINK_OPENER))
                        .with(
                                AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT,
                                "Enroll virtual card to make it more secure?")
                        .build();
        PropertyModelChangeProcessor.create(
                model, view, AutofillVcnEnrollBottomSheetViewBinder::bind);
        mVcnEnrollBottomSheetContent =
                new AutofillVcnEnrollBottomSheetContent(
                        view.mContentView, view.mScrollView, /* onDismiss= */ () -> {});

        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController.requestShowContent(
                            mVcnEnrollBottomSheetContent, /* animate= */ false);
                });
        ViewGroup bottomSheetView =
                mTabbedActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Render the bottom sheet to show the content sheet and bottom sheet background.
        mRenderTestRule.render(bottomSheetView, "vcn_enroll_bottom_sheet_content");

        // Scroll to show the bottom of the contents.
        runOnUiThreadBlocking(
                () -> {
                    view.mScrollView.scrollTo(/* x= */ 0, (int) view.mCancelButton.getY());
                });

        // Render the bottom sheet to show the scrolled content sheet and bottom sheet background.
        mRenderTestRule.render(bottomSheetView, "vcn_enroll_bottom_sheet_content_scrolled");
    }
}
