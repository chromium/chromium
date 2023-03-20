// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/** Tests {@link ShareButtonController}. */

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures(
        {ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR, ChromeFeatureList.START_SURFACE_ANDROID})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.START_SURFACE_ANDROID + "<Study",
        "force-fieldtrials=Study/Group"})
public final class ShareButtonControllerTest {
    private final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final SigninTestRule mSigninTestRule = new SigninTestRule();

    // Mock sign-in environment needs to be destroyed after ChromeActivity in case there are
    // observers registered in the AccountManagerFacade mock.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    private boolean mButtonExpected;

    @Before
    public void setUp() {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(AdaptiveToolbarButtonVariant.SHARE);
        mActivityTestRule.startMainActivityOnBlankPage();

        int deviceWidth =
                mActivityTestRule.getActivity().getResources().getConfiguration().screenWidthDp;

        mButtonExpected =
                deviceWidth >= AdaptiveToolbarFeatures.getDeviceMinimumWidthForShowingButton();
    }

    @Test
    @MediumTest
    public void testShareButtonInToolbarIsDisabledOnStartNTP() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        ChromeTabUtils.waitForTabPageLoaded(
                mActivityTestRule.getActivity().getActivityTab(), UrlConstants.NTP_URL);

        View experimentalButton = mActivityTestRule.getActivity()
                                          .getToolbarManager()
                                          .getToolbarLayoutForTesting()
                                          .getOptionalButtonViewForTesting();
        if (experimentalButton != null) {
            String shareString =
                    mActivityTestRule.getActivity().getResources().getString(R.string.share);
            assertTrue("Share button isnt showing",
                    (View.GONE == experimentalButton.getVisibility()
                            || !shareString.equals(experimentalButton.getContentDescription())));
        }
    }

    @Test
    @MediumTest
    public void testShareButtonInToolbarIsEnabledOnBlankPage() {
        View experimentalButton = mActivityTestRule.getActivity()
                                          .getToolbarManager()
                                          .getToolbarLayoutForTesting()
                                          .getOptionalButtonViewForTesting();

        if (!mButtonExpected) {
            assertTrue(
                    experimentalButton == null || View.GONE == experimentalButton.getVisibility());
        } else {
            assertNotNull("experimental button not found", experimentalButton);
            assertEquals(View.VISIBLE, experimentalButton.getVisibility());
            String shareString =
                    mActivityTestRule.getActivity().getResources().getString(R.string.share);

            assertEquals(shareString, experimentalButton.getContentDescription());
        }
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @Restriction(
            {UiRestriction.RESTRICTION_TYPE_PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @DisabledTest(message = "crbug.com/1381572")
    public void
    testShareButtonInToolbarNotAffectedByOverview() throws TimeoutException {
        // Sign in.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mActivityTestRule.getActivity().getStartSurface().setStartSurfaceState(
                                StartSurfaceState.SHOWING_START));
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, false);

        View optionalButton = mActivityTestRule.getActivity()
                                      .getToolbarManager()
                                      .getToolbarLayoutForTesting()
                                      .getOptionalButtonViewForTesting();
        if (!mButtonExpected) {
            assertTrue(optionalButton == null || View.GONE == optionalButton.getVisibility());
        } else {
            assertNotNull("optional button not found", optionalButton);

            String shareString =
                    mActivityTestRule.getActivity().getResources().getString(R.string.share);

            assertEquals(shareString, optionalButton.getContentDescription());
        }
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1381572")
    public void testShareButtonInToolbarIsDisabledOnUpdate() {
        View experimentalButton = mActivityTestRule.getActivity()
                                          .getToolbarManager()
                                          .getToolbarLayoutForTesting()
                                          .getOptionalButtonViewForTesting();

        ModalDialogProperties.Controller controller = new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {}

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        };

        PropertyModel dialogModel = TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                   .with(ModalDialogProperties.CONTROLLER, controller)
                                   .build());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getModalDialogManager().showDialog(
                    dialogModel, ModalDialogType.APP);
        });

        if (!mButtonExpected) {
            assertTrue(
                    experimentalButton == null || View.GONE == experimentalButton.getVisibility());
        } else {
            assertNotNull("experimental button not found", experimentalButton);
            assertEquals(View.VISIBLE, experimentalButton.getVisibility());
            String shareString =
                    mActivityTestRule.getActivity().getResources().getString(R.string.share);

            assertTrue(shareString.equals(experimentalButton.getContentDescription()));
            assertFalse(experimentalButton.isEnabled());
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getModalDialogManager().dismissDialog(
                    dialogModel, DialogDismissalCause.UNKNOWN);
        });
        if (!mButtonExpected) {
            assertTrue(
                    experimentalButton == null || View.GONE == experimentalButton.getVisibility());
        } else {
            assertNotNull("experimental button not found", experimentalButton);
            assertEquals(View.VISIBLE, experimentalButton.getVisibility());
            String shareString =
                    mActivityTestRule.getActivity().getResources().getString(R.string.share);

            assertTrue(shareString.equals(experimentalButton.getContentDescription()));
            assertTrue(experimentalButton.isEnabled());
        }
    }

    // TODO(crbug/1036023) Add a test that checks that expected intents are fired.
}
