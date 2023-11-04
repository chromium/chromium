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
import android.os.Build;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout.Orientation;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerImpl;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeOSWrapperImpl;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.UiRestriction;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
@Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
@MinAndroidSdkLevel(Build.VERSION_CODES.R)
@EnableFeatures({ChromeFeatureList.DRAW_CUTOUT_EDGE_TO_EDGE, ChromeFeatureList.DRAW_EDGE_TO_EDGE})
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

    private class TestOsWrapper extends EdgeToEdgeOSWrapperImpl {
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

    @Before
    public void setUp() {
        mTestServer = sActivityTestRule.getTestServer();
        mActivity = sActivityTestRule.getActivity();
        assertNotNull(mActivity);

        mEdgeToEdgeController =
                (EdgeToEdgeControllerImpl)
                        mActivity
                                .getRootUiCoordinatorForTesting()
                                .getEdgeToEdgeControllerForTesting();
        assertNotNull("Couldn't get the EdgeToEdgeController during setUp!", mEdgeToEdgeController);
        assertFalse(
                "Setup error, all tests start ToNormal (controller never activated)!",
                mEdgeToEdgeController.isToEdge());
        mTestOsWrapper = new TestOsWrapper();
        mEdgeToEdgeController.setOsWrapperForTesting(mTestOsWrapper);
        mTestOsWrapper.resetPaddingMonitor();
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
        waitTillToEdge();
        assertTrue("Helper goToEdge failed to go ToEdge", mEdgeToEdgeController.isToEdge());
    }

    /** Puts the screen ToNormal by loading a page that has the appropriate HTML. */
    void goToNormal() {
        sActivityTestRule.loadUrl(mTestServer.getURL(TEST_AUTO_PAGE));
        waitTillToNormal();
        assertFalse("Helper goToNormal failed to go ToNormal", mEdgeToEdgeController.isToEdge());
    }

    void waitTillToEdge() {
        CriteriaHelper.pollUiThread(() -> mEdgeToEdgeController.isToEdge());
    }

    void waitTillToNormal() {
        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeController.isToEdge());
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
    public void testRotationToLandscape_WhileToNormal() {
        // We must activate ToEdge to test the closure capturing the local toEdge value instead of
        // the updated private member and passing that to adjustEdges.
        activateFeatureToEdge();
        goToNormal();
        assertEquals(
                "This test should start in portrait orientation!",
                Orientation.PORTRAIT,
                mActivity.getResources().getConfiguration().orientation);

        int targetOrientation = Configuration.ORIENTATION_LANDSCAPE;
        rotate(targetOrientation);
        assertFalse("Rotation did not preserve ToNormal setting", mEdgeToEdgeController.isToEdge());
        assertNotEquals(
                "Padding indicates ToEdge, which is inconsistent with the Controller"
                        + " thinking we're ToNormal!",
                TO_EDGE_PADDING,
                mTestOsWrapper.getNextPadding());
    }

    @Test
    @MediumTest
    public void testRotationToLandscape_WhileToEdge() {
        activateFeatureToEdge();
        assertEquals(
                "This test should start in portrait orientation!",
                Orientation.PORTRAIT,
                mActivity.getResources().getConfiguration().orientation);

        int targetOrientation = Configuration.ORIENTATION_LANDSCAPE;
        rotate(targetOrientation);
        assertTrue("Rotation did not preserve ToEdge setting", mEdgeToEdgeController.isToEdge());
        assertEquals(
                "Padding indicates ToNormal, which is inconsistent with the Controller"
                        + " thinking we're ToEdge!",
                TO_EDGE_PADDING,
                mTestOsWrapper.getNextPadding());
    }

    @Test
    @MediumTest
    public void testRotationToPortrait_WhileToNormal() {
        activateFeatureToEdge();
        rotate(Configuration.ORIENTATION_LANDSCAPE);
        goToNormal();

        int targetOrientation = Configuration.ORIENTATION_PORTRAIT;
        rotate(targetOrientation);
        assertFalse("Rotation did not preserve ToNormal setting", mEdgeToEdgeController.isToEdge());
        assertNotEquals(
                "Padding indicates ToEdge, which is inconsistent with the Controller"
                        + " thinking we're ToNormal!",
                TO_EDGE_PADDING,
                mTestOsWrapper.getNextPadding());
    }

    @Test
    @MediumTest
    public void testRotationToPortrait_WhileToEdge() {
        activateFeatureToEdge();
        rotate(Configuration.ORIENTATION_LANDSCAPE);

        int targetOrientation = Configuration.ORIENTATION_PORTRAIT;
        rotate(targetOrientation);
        assertTrue("Rotation did not preserve ToEdge setting", mEdgeToEdgeController.isToEdge());
        assertEquals(
                "Padding indicates ToNormal, which is inconsistent with the Controller"
                        + " thinking we're ToEdge!",
                TO_EDGE_PADDING,
                mTestOsWrapper.getNextPadding());
    }
}
