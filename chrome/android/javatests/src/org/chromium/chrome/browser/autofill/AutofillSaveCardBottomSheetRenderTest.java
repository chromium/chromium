// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.graphics.Color;
import android.view.ViewGroup;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.autofill.payments.CardDetail;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.autofill.payments.LegalMessageLine.Link;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.Arrays;
import java.util.Collections;

@LargeTest
@Batch(Batch.UNIT_TESTS)
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({AutofillFeatures.AUTOFILL_ENABLE_PAYMENTS_ANDROID_BOTTOM_SHEET})
public class AutofillSaveCardBottomSheetRenderTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(0)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    private BottomSheetController mBottomSheetController;
    @Mock private AutofillSaveCardBottomSheetContent.Delegate mSaveCardBottomSheetContentDelegate;
    private AutofillSaveCardBottomSheetContent mSaveCardBottomSheetContent;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(/* startIntent= */ null);
    }

    @Before
    public void setUp() {
        runOnUiThreadBlocking(
                () -> {
                    Activity activity = sActivityTestRule.getActivity();
                    ViewGroup activityContentView = activity.findViewById(android.R.id.content);
                    activityContentView.removeAllViews();
                    ScrimCoordinator scrimCoordinator =
                            new ScrimCoordinator(
                                    activity,
                                    new ScrimCoordinator.SystemUiScrimDelegate() {
                                        @Override
                                        public void setStatusBarScrimFraction(
                                                float scrimFraction) {}

                                        @Override
                                        public void setNavigationBarScrimFraction(
                                                float scrimFraction) {}
                                    },
                                    activityContentView,
                                    Color.WHITE);
                    mBottomSheetController =
                            BottomSheetControllerFactory.createFullWidthBottomSheetController(
                                    () -> scrimCoordinator,
                                    (unused) -> {},
                                    activity.getWindow(),
                                    KeyboardVisibilityDelegate.getInstance(),
                                    () -> activityContentView);

                    mSaveCardBottomSheetContent =
                            new AutofillSaveCardBottomSheetContent(sActivityTestRule.getActivity());
                    mSaveCardBottomSheetContent.setDelegate(mSaveCardBottomSheetContentDelegate);
                });
    }

    @After
    public void tearDown() {
        runOnUiThreadBlocking(
                () -> {
                    if (mBottomSheetController.getCurrentSheetContent() != null) {
                        mBottomSheetController.hideContent(
                                mSaveCardBottomSheetContent, /* animate= */ false);
                    }
                });
    }

    @Test
    @Feature({"RenderTest"})
    public void testUploadSave() throws Exception {
        mSaveCardBottomSheetContent.setUiInfo(
                new AutofillSaveCardUiInfo.Builder()
                        .withIsForUpload(true)
                        .withLogoIcon(R.drawable.google_pay)
                        .withCardDetail(
                                new CardDetail(R.drawable.visa_card, "Card label", "Card sublabel"))
                        .withLegalMessageLines(
                                Arrays.asList(
                                        new LegalMessageLine(
                                                "Legal message line #1",
                                                Arrays.asList(
                                                        new Link(
                                                                /* start= */ 0,
                                                                /* end= */ 5,
                                                                /* url= */ "https://example.com"))),
                                        new LegalMessageLine("Legal message line #2")))
                        .withTitleText("Title text")
                        .withConfirmText("Confirm text")
                        .withCancelText("Cancel text")
                        .withIsGooglePayBrandingEnabled(true)
                        .withDescriptionText("Description text.")
                        .build());
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController.requestShowContent(
                            mSaveCardBottomSheetContent, /* animate= */ false);
                });
        ViewGroup activityContentView =
                sActivityTestRule.getActivity().findViewById(android.R.id.content);
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Render the activity to show the content sheet and its contents.
        mRenderTestRule.render(activityContentView, "save_card_bottom_sheet_content_upload");
    }

    @Test
    @Feature({"RenderTest"})
    public void testLocalSave() throws Exception {
        mSaveCardBottomSheetContent.setUiInfo(
                new AutofillSaveCardUiInfo.Builder()
                        .withIsForUpload(false)
                        .withLogoIcon(R.drawable.arrow_up) // The logo should not be shown.
                        .withCardDetail(
                                new CardDetail(R.drawable.visa_card, "Card label", "Card sublabel"))
                        .withLegalMessageLines(Collections.emptyList()) // No legal message
                        // lines on local save.
                        .withTitleText("Title text")
                        .withConfirmText("Confirm text")
                        .withCancelText("Cancel text")
                        .withIsGooglePayBrandingEnabled(false)
                        .withDescriptionText("") // Description text is empty on local save.
                        .build());
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController.requestShowContent(
                            mSaveCardBottomSheetContent, /* animate= */ false);
                });
        ViewGroup activityContentView =
                sActivityTestRule.getActivity().findViewById(android.R.id.content);
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Render the activity to show the content sheet and its contents.
        mRenderTestRule.render(activityContentView, "save_card_bottom_sheet_content_local");
    }
}
