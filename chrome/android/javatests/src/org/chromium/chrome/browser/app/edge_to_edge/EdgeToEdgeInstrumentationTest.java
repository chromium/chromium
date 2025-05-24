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

import androidx.core.graphics.Insets;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
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
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.ui.test.util.RenderTestRule.Corpus;

import java.io.IOException;

@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Testing startup behavior")
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
@Restriction({DeviceFormFactor.PHONE, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
@MinAndroidSdkLevel(Build.VERSION_CODES.R)
@EnableFeatures({
    ChromeFeatureList.DRAW_CUTOUT_EDGE_TO_EDGE,
    ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE,
    ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
    ChromeFeatureList.EDGE_TO_EDGE_WEB_OPT_IN
})
public class EdgeToEdgeInstrumentationTest {
    @Rule
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule
    public final RenderTestRule renderTestRule =
            new RenderTestRule.Builder()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_EDGE_TO_EDGE)
                    .setCorpus(Corpus.ANDROID_RENDER_TESTS_PUBLIC)
                    .setRevision(0)
                    .build();

    private static final String TEST_AUTO_PAGE =
            "/chrome/test/data/android/edge_to_edge/viewport-fit-auto.html";
    private static final String TEST_COVER_PAGE =
            "/chrome/test/data/android/edge_to_edge/viewport-fit-cover.html";
    private static final String TEST_CONTAIN_PAGE =
            "/chrome/test/data/android/edge_to_edge/viewport-fit-contain.html";

    private EdgeToEdgeControllerImpl mEdgeToEdgeController;

    private EmbeddedTestServer mTestServer;
    private ChromeTabbedActivity mActivity;

    // Declare the watcher before the app launches.
    HistogramWatcher mEligibleHistograms =
            HistogramWatcher.newBuilder()
                    .expectBooleanRecord("Android.EdgeToEdge.Eligible", true)
                    .expectNoRecords("Android.EdgeToEdge.IneligibilityReason")
                    .build();

    @Before
    public void setUp() {
        mTestServer = mActivityTestRule.getTestServer();
        mActivity = mActivityTestRule.getActivity();
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
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_COVER_PAGE));
        waitUntilOptedIntoEdgeToEdge();
        assertTrue("Helper goToEdge failed to go ToEdge", mEdgeToEdgeController.isDrawingToEdge());
        assertTrue(
                "Helper goToEdge failed to opt into EdgeToEdge",
                mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
    }

    /** Puts the screen ToNormal by loading a page that has the appropriate HTML. */
    void optOutOfToEdge() {
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_AUTO_PAGE));
        waitUntilNotOptedIntoEdgeToEdge();
        assertFalse(
                "Helper optOutOfToEdge failed to stop opting into E2E",
                mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
    }

    void loadSafeAreaConstrainPage() {
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_CONTAIN_PAGE));
        waitUntilNotOptedIntoEdgeToEdge();
        assertFalse(
                "Helper loadSafeAreaConstrainPage failed to stop opting into E2E",
                mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
        assertTrue(
                "Safe area constraint should be set for contain pages.",
                mEdgeToEdgeController.getHasSafeAreaConstraintForTesting());
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
    @DisableFeatures(ChromeFeatureList.FLOATING_SNACKBAR)
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
    @EnableFeatures(ChromeFeatureList.FLOATING_SNACKBAR)
    public void testFloatingSnackbar() throws InterruptedException {
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
        Assert.assertNull(
                "Pad Adjuster is not used in the floating snackbar and should be null.", adjuster);
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
        assertNavigationBarColor(mActivity.getActivityTab().getBackgroundColor());

        TabUiTestHelper.enterTabSwitcher(mActivity);
        assertEquals(
                "Should still be drawing toEdge in the Tab Switcher.",
                Color.TRANSPARENT,
                mActivity.getWindow().getNavigationBarColor());

        TabUiTestHelper.leaveTabSwitcher(mActivity);
        assertEquals(
                "Should stay toEdge upon leaving the Tab Switcher.",
                Color.TRANSPARENT,
                mActivity.getWindow().getNavigationBarColor());
        assertNavigationBarColor(mActivity.getActivityTab().getBackgroundColor());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_SAFE_AREA_CONSTRAINT)
    public void testSafeAreaConstraint() {
        loadSafeAreaConstrainPage();

        int bottomInsets = mEdgeToEdgeController.getBottomInsetPx();
        int bottomControlsMinHeight =
                mActivity
                        .getRootUiCoordinatorForTesting()
                        .getBottomControlsStackerForTesting()
                        .getTotalMinHeight();
        assertEquals(
                "Bottom controls min height should be set as the height of the bottom insets.",
                bottomInsets,
                bottomControlsMinHeight);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE + ":e2e_everywhere_debug/true")
    public void testPadWithEdgeToEdgeLayout() throws IOException {
        goToEdge();
        assertDrawingToEdge();

        Insets appliedPadding = mEdgeToEdgeController.getAppliedContentViewPaddingForTesting();
        assertEquals("Bottom padding is 0 when drawing to edge.", 0, appliedPadding.bottom);
        assertNotEquals(
                "Top padding is not 0, but should be handled with e2e layout.",
                0,
                appliedPadding.top);

        // Padding is verified by the debug layer for e2e layout in render golden's result.
        // Expect to see a magenta color block on top of the toolbar.
        renderTestRule.render(
                mActivity.findViewById(android.R.id.content), "e2e-everywhere-no-bottom-padding");
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
    }

    private void assertNavigationBarColor(int color) {
        assertEquals(
                "Nav bar color is different.",
                color,
                mActivity
                        .getEdgeToEdgeManager()
                        .getEdgeToEdgeSystemBarColorHelper()
                        .getNavigationBarColor());
    }
}
