// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.view.KeyEvent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.net.test.EmbeddedTestServer;

/** Tests for zooming into & out of a selected & deselected editable text field. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
public class FocusedEditableTextFieldZoomTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final int TEST_TIMEOUT = 5000;
    private static final String TEXTFIELD_DOM_ID = "textfield";
    private static final float FLOAT_DELTA = 0.01f;
    private static final float INITIAL_SCALE = 0.5f;

    private EmbeddedTestServer mTestServer;
    private Coordinates mCoordinates;

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/focused_editable_zoom.html"));
        mCoordinates = Coordinates.createFor(mActivityTestRule.getWebContents());
        waitForInitialZoom();
    }

    private void waitForInitialZoom() {
        // The zoom level sometimes changes immediately after the page loads which makes grabbing
        // the initial value problematic. We solve this by explicitly specifying the initial zoom
        // level via the viewport tag and waiting for the zoom level to reach that value before we
        // proceed with the rest of the test.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            (double) mCoordinates.getPageScaleFactor(),
                            Matchers.is(Matchers.closeTo(INITIAL_SCALE, FLOAT_DELTA)));
                },
                TEST_TIMEOUT,
                DEFAULT_POLLING_INTERVAL);
    }

    private void waitForZoomIn(final float initialZoomLevel) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            mCoordinates.getPageScaleFactor(),
                            Matchers.greaterThan(initialZoomLevel));
                },
                TEST_TIMEOUT,
                DEFAULT_POLLING_INTERVAL);
    }

    /*
     * @LargeTest
     */
    @Test
    @DisabledTest(message = "Broken by subpixel precision changes crbug.com/371119")
    @Feature({"TabContents"})
    public void testZoomInToSelected() throws Throwable {
        // This should focus the text field and initiate a zoom in.
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        float initialZoomLevel = mCoordinates.getPageScaleFactor();

        DOMUtils.clickNode(tab.getWebContents(), TEXTFIELD_DOM_ID);

        // Wait for the zoom in to complete.
        waitForZoomIn(initialZoomLevel);
    }

    /*
     * @LargeTest
     */
    @Test
    @DisabledTest(message = "Broken by subpixel precision changes crbug.com/371119")
    @Feature({"TabContents"})
    public void testZoomOutOfSelectedIfOnlyBackPressed() throws Throwable {
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final float initialZoomLevel = mCoordinates.getPageScaleFactor();

        // This should focus the text field and initiate a zoom in.
        DOMUtils.clickNode(tab.getWebContents(), TEXTFIELD_DOM_ID);

        // Wait for the zoom in to complete.
        waitForZoomIn(initialZoomLevel);

        KeyUtils.singleKeyEventView(
                InstrumentationRegistry.getInstrumentation(), tab.getView(), KeyEvent.KEYCODE_BACK);

        // We should zoom out to the previous zoom level.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            (double) mCoordinates.getPageScaleFactor(),
                            Matchers.is(Matchers.closeTo(initialZoomLevel, FLOAT_DELTA)));
                },
                TEST_TIMEOUT,
                DEFAULT_POLLING_INTERVAL);
    }
}
