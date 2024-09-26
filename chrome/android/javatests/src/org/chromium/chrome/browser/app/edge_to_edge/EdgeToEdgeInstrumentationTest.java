// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.edge_to_edge;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.res.Configuration;
import android.graphics.Color;
import android.os.Build;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout.Orientation;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerImpl;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeOSWrapperImpl;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Testing startup behavior")
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
@Restriction({DeviceFormFactor.PHONE, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
@MinAndroidSdkLevel(Build.VERSION_CODES.R)
@EnableFeatures({
    ChromeFeatureList.DRAW_CUTOUT_EDGE_TO_EDGE,
    ChromeFeatureList.DRAW_EDGE_TO_EDGE,
    ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
    ChromeFeatureList.EDGE_TO_EDGE_WEB_OPT_IN
})
public class EdgeToEdgeInstrumentationTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String TEST_AUTO_PAGE =
            "/chrome/test/data/android/edge_to_edge/viewport-fit-auto.html";
    private static final String TEST_COVER_PAGE =
            "/chrome/test/data/android/edge_to_edge/viewport-fit-cover.html";

    private static final int TO_EDGE_PADDING = 0;

    private EdgeToEdgeControllerImpl mEdgeToEdgeController;

    private EmbeddedTestServer mTestServer;
    private ChromeTabbedActivity mActivity;

    private TestOsWrapper mTestOsWrapper;

    private static class TestOsWrapper extends EdgeToEdgeOSWrapperImpl {
        boolean mDidSetBottomPadding;
        int mBottomPadding;

        @Override
        public void setPadding(View view, int left, int top, int right, int bottom) {
            mDidSetBottomPadding = true;
            mBottomPadding = bottom;
            super.setPadding(view, left, top, right, bottom);
        }

        void resetPaddingMonitor() {
            mDidSetBottomPadding = false;
        }

        int getNextPadding() {
            CriteriaHelper.pollUiThread(() -> mDidSetBottomPadding);
            return mBottomPadding;
        }
    }

    // Declare the watcher before the app launches.
    HistogramWatcher mEligibleHistograms =
            HistogramWatcher.newBuilder()
                    .expectBooleanRecord("Android.EdgeToEdge.Eligible", true)
                    .expectNoRecords("Android.EdgeToEdge.IneligibilityReason")
                    .build();

    @Before
    public void setUp() {
        mTestServer = sActivityTestRule.getTestServer();
        mActivity = sActivityTestRule.getActivity();
        assertNotNull(mActivity);

        CriteriaHelper.pollUiThread(
                () -> {
                    mEdgeToEdgeController =
                            (EdgeToEdgeControllerImpl)
                                    mActivity.getEdgeToEdgeControllerSupplierForTesting().get();
                    return mEdgeToEdgeController != null;
                },
                "Couldn't get the EdgeToEdgeController during setUp!");
        assertFalse(
                "Setup error, all tests start not opted into edge-to-edge!",
                mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
        mTestOsWrapper = new TestOsWrapper();
        mEdgeToEdgeController.setOsWrapperForTesting(mTestOsWrapper);
        mTestOsWrapper.resetPaddingMonitor();
        EdgeToEdgeControllerFactory.setHas3ButtonNavBar(false);
    }

    @After
    public void tearDown() {
        if (mActivity.getResources().getConfiguration().orientation
                != Configuration.ORIENTATION_PORTRAIT) {
            rotate(Configuration.ORIENTATION_PORTRAIT);
        }
    }

    /** Puts the screen ToEdge by loading a page that has the appropriate HTML. */
    void goToEdge() {
        sActivityTestRule.loadUrl(mTestServer.getURL(TEST_COVER_PAGE));
        waitUntilOptedIntoEdgeToEdge();
        assertTrue("Helper goToEdge failed to go ToEdge", mEdgeToEdgeController.isDrawingToEdge());
        assertTrue(
                "Helper goToEdge failed to opt into EdgeToEdge",
                mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
    }

    /** Puts the screen ToNormal by loading a page that has the appropriate HTML. */
    void optOutOfToEdge() {
        sActivityTestRule.loadUrl(mTestServer.getURL(TEST_AUTO_PAGE));
        waitUntilNotOptedIntoEdgeToEdge();
        assertFalse(
                "Helper optOutOfToEdge failed to stop opting into E2E",
                mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
    }

    void waitUntilOptedIntoEdgeToEdge() {
        CriteriaHelper.pollUiThread(() -> mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
    }

    void waitUntilNotOptedIntoEdgeToEdge() {
        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
    }

    /** Rotates the device to the given orientation. */
    void rotate(int targetOrientation) {
        ActivityTestUtils.rotateActivityToOrientation(mActivity, targetOrientation);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivity.getResources().getConfiguration().orientation,
                            is(targetOrientation));
                });
    }

    /** Activates the Edge To Edge ability to draw under System Bars. */
    void activateFeatureToEdge() {
        // Trigger lazy activation of the ability to draw under System Bars.
        goToEdge();
    }

    /** Tests a failure case when rotating while ToNormal after going ToEdge. */
    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
    public void testRotationToLandscape_WhileNotOptedIntoE2E_BottomChinDisabled() {
        // We must activate ToEdge to test the closure capturing the local toEdge value instead of
        // the updated private member and passing that to adjustEdges.
        activateFeatureToEdge();
        optOutOfToEdge();
        assertEquals(
                "This test should start in portrait orientation!",
                Orientation.PORTRAIT,
                mActivity.getResources().getConfiguration().orientation);

        int targetOrientation = Configuration.ORIENTATION_LANDSCAPE;
        rotate(targetOrientation);

        assertNotOptedIntoEdgeToEdge();
        assertFalse(
                "Rotation did not preserve ToNormal setting",
                mEdgeToEdgeController.isDrawingToEdge());
        assertNotEquals(
                "Padding indicates ToEdge, which is inconsistent with the Controller"
                        + " thinking we're ToNormal!",
                TO_EDGE_PADDING,
                mTestOsWrapper.getNextPadding());
    }

    /** Tests a failure case when rotating while ToNormal after going ToEdge. */
    @Test
    @MediumTest
    public void testRotationToLandscape_WhileNotOptedIntoE2E() {
        // We must activate ToEdge to test the closure capturing the local toEdge value instead of
        // the updated private member and passing that to adjustEdges.
        activateFeatureToEdge();
        optOutOfToEdge();
        assertEquals(
                "This test should start in portrait orientation!",
                Orientation.PORTRAIT,
                mActivity.getResources().getConfiguration().orientation);

        int targetOrientation = Configuration.ORIENTATION_LANDSCAPE;
        rotate(targetOrientation);

        assertNotOptedIntoEdgeToEdge();
        assertDrawingToEdge();
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
    public void testRotationToLandscape_WhileOptedIntoE2E_BottomChinDisabled() {
        activateFeatureToEdge();
        assertEquals(
                "This test should start in portrait orientation!",
                Orientation.PORTRAIT,
                mActivity.getResources().getConfiguration().orientation);

        int targetOrientation = Configuration.ORIENTATION_LANDSCAPE;
        rotate(targetOrientation);

        assertOptedIntoEdgeToEdge();
        assertDrawingToEdge();
    }

    @Test
    @MediumTest
    public void testRotationToLandscape_WhileOptedIntoE2E() {
        activateFeatureToEdge();
        assertEquals(
                "This test should start in portrait orientation!",
                Orientation.PORTRAIT,
                mActivity.getResources().getConfiguration().orientation);

        int targetOrientation = Configuration.ORIENTATION_LANDSCAPE;
        rotate(targetOrientation);

        assertOptedIntoEdgeToEdge();
        assertDrawingToEdge();
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
    public void testRotationToPortrait_WhileNotOptedIntoE2E_BottomChinDisabled() {
        activateFeatureToEdge();
        rotate(Configuration.ORIENTATION_LANDSCAPE);
        optOutOfToEdge();

        int targetOrientation = Configuration.ORIENTATION_PORTRAIT;
        rotate(targetOrientation);

        assertNotOptedIntoEdgeToEdge();
        assertFalse(
                "Rotation did not preserve ToNormal setting",
                mEdgeToEdgeController.isDrawingToEdge());
        assertNotEquals(
                "Padding indicates ToEdge, which is inconsistent with the Controller"
                        + " thinking we're ToNormal!",
                TO_EDGE_PADDING,
                mTestOsWrapper.getNextPadding());
    }

    @Test
    @MediumTest
    public void testRotationToPortrait_WhileNotOptedIntoE2E() {
        activateFeatureToEdge();
        rotate(Configuration.ORIENTATION_LANDSCAPE);
        optOutOfToEdge();

        int targetOrientation = Configuration.ORIENTATION_PORTRAIT;
        rotate(targetOrientation);

        assertNotOptedIntoEdgeToEdge();
        assertDrawingToEdge();
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
    public void testRotationToPortrait_WhileOptedIntoE2E_BottomChinDisabled() {
        activateFeatureToEdge();
        rotate(Configuration.ORIENTATION_LANDSCAPE);

        int targetOrientation = Configuration.ORIENTATION_PORTRAIT;
        rotate(targetOrientation);

        assertOptedIntoEdgeToEdge();
        assertDrawingToEdge();
    }

    @Test
    @MediumTest
    public void testRotationToPortrait_WhileOptedIntoE2E() {
        activateFeatureToEdge();
        rotate(Configuration.ORIENTATION_LANDSCAPE);

        int targetOrientation = Configuration.ORIENTATION_PORTRAIT;
        rotate(targetOrientation);

        assertOptedIntoEdgeToEdge();
        assertDrawingToEdge();
    }

    @Test
    @MediumTest
    public void testSnackbar() throws InterruptedException {
        activateFeatureToEdge();
        optOutOfToEdge();
        var snackbarManager = mActivity.getSnackbarManager();
        snackbarManager.setEdgeToEdgeSupplier(mEdgeToEdgeController);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    snackbarManager.showSnackbar(
                            Snackbar.make(
                                    "Test",
                                    new SnackbarManager.SnackbarController() {},
                                    Snackbar.TYPE_PERSISTENT,
                                    Snackbar.UMA_TEST_SNACKBAR));
                });

        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        var adjuster =
                snackbarManager
                        .getCurrentSnackbarViewForTesting()
                        .getEdgeToEdgePadAdjusterForTesting();
        Assert.assertNotNull("Pad Adjuster should be created", adjuster);

        int heightOnAuto =
                snackbarManager.getCurrentSnackbarViewForTesting().getViewForTesting().getHeight();

        goToEdge();
        int heightOnCover =
                snackbarManager.getCurrentSnackbarViewForTesting().getViewForTesting().getHeight();
        Assert.assertEquals(
                "New padding has been added to adjusters when viewport-fit=cover.",
                mEdgeToEdgeController.getBottomInsetPx(),
                heightOnCover - heightOnAuto);

        optOutOfToEdge();
        heightOnAuto =
                snackbarManager.getCurrentSnackbarViewForTesting().getViewForTesting().getHeight();
        Assert.assertEquals(
                "Padding to adjusters has been removed when viewport-fit=auto.",
                mEdgeToEdgeController.getBottomInsetPx(),
                heightOnCover - heightOnAuto);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/41492043")
    public void testUnfold() {
        activateFeatureToEdge();
        assertEquals(
                "This test should start in portrait orientation!",
                Orientation.PORTRAIT,
                mActivity.getResources().getConfiguration().orientation);

        // Set 3-button mode to simulate switching to a tablet.
        // Using a mocked static EdgeToEdgeControllerFactory#isSupportedConfiguration would be
        // better but they are not supported on Android by Mockito.
        EdgeToEdgeControllerFactory.setHas3ButtonNavBar(true);

        // Use an orientation change to trigger new insets.
        int targetOrientation = Configuration.ORIENTATION_LANDSCAPE;
        rotate(targetOrientation);
        assertFalse(
                "Should exit ToEdge when device no longer supported",
                mEdgeToEdgeController.isDrawingToEdge());
    }

    @Test
    @MediumTest
    public void testEligibleHistogramRecord() {
        UserActionTester userActionTester = new UserActionTester();
        activateFeatureToEdge();

        Assert.assertTrue(
                "User action is not recorded",
                userActionTester.getActions().contains("MobilePageLoadedWithToEdge"));
        mEligibleHistograms.assertExpected("Incorrect histogram recordings");
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
    public void testNavigationBarColor_BottomChinDisabled() {
        optOutOfToEdge();

        goToEdge();
        assertEquals(
                "Navigation bar should be transparent in edge to edge.",
                Color.TRANSPARENT,
                mActivity.getWindow().getNavigationBarColor());

        optOutOfToEdge();
        assertEquals(
                "Navigation bar should have the right color when transitioning away from edge to"
                        + " edge,",
                mActivity.getActivityTab().getBackgroundColor(),
                mActivity.getWindow().getNavigationBarColor());
    }

    @Test
    @MediumTest
    public void testNavigationBarColor() {
        optOutOfToEdge();

        goToEdge();
        assertEquals(
                "Navigation bar should be transparent in edge to edge.",
                Color.TRANSPARENT,
                mActivity.getWindow().getNavigationBarColor());

        optOutOfToEdge();
        assertEquals(
                "Navigation bar should stay transparent for the bottom chin even when not"
                        + "opted in.",
                Color.TRANSPARENT,
                mActivity.getWindow().getNavigationBarColor());

        TabUiTestHelper.enterTabSwitcher(mActivity);
        assertNotEquals(
                "Should not be drawing toEdge in the Tab Switcher.",
                Color.TRANSPARENT,
                mActivity.getWindow().getNavigationBarColor());

        TabUiTestHelper.leaveTabSwitcher(mActivity);
        assertEquals(
                "Should return toEdge upon leaving the Tab Switcher.",
                Color.TRANSPARENT,
                mActivity.getWindow().getNavigationBarColor());
    }

    private void assertOptedIntoEdgeToEdge() {
        assertTrue(
                "Rotation did not preserve E2E opt-in setting",
                mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
    }

    private void assertNotOptedIntoEdgeToEdge() {
        assertFalse(
                "Rotation did not preserve E2E opt-out setting",
                mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
    }

    private void assertDrawingToEdge() {
        assertTrue(
                "Rotation did not preserve ToEdge setting. The device should still be drawing"
                        + " ToEdge for the bottom chin.",
                mEdgeToEdgeController.isDrawingToEdge());
        assertEquals(
                "Padding indicates ToNormal, which is inconsistent with the Controller"
                        + " thinking we're ToEdge!",
                TO_EDGE_PADDING,
                mTestOsWrapper.mBottomPadding);
    }
}
