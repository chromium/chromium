// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.hamcrest.Matchers.lessThanOrEqualTo;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests the virtual keyboard's effect on resizing web pages.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
@Batch(Batch.PER_CLASS)
public class VirtualKeyboardResizeTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEXTFIELD_DOM_ID = "inputElement";
    private static final int TEST_TIMEOUT = 10000;

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private void startMainActivityWithURL(String url) {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(url));
    }

    private void assertWaitForKeyboardStatus(final boolean show) {
        CriteriaHelper.pollUiThread(() -> {
            boolean isKeyboardShowing = mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                    mActivityTestRule.getActivity(), mActivityTestRule.getActivity().getTabsView());
            Criteria.checkThat(isKeyboardShowing, Matchers.is(show));
        }, TEST_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void assertWaitForPageHeight(Matcher<java.lang.Integer> matcher) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                int curHeight = getPageInnerHeight();
                Criteria.checkThat(curHeight, matcher);
            } catch (Throwable e) {
                throw new CriteriaNotSatisfiedException(e);
            }
        }, TEST_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void assertWaitForVisualViewportHeight(Matcher<java.lang.Double> matcher) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                double curHeight = getVisualViewportHeight();
                Criteria.checkThat(curHeight, matcher);
            } catch (Throwable e) {
                throw new CriteriaNotSatisfiedException(e);
            }
        }, TEST_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private WebContents getWebContents() {
        return mActivityTestRule.getActivity().getActivityTab().getWebContents();
    }

    private int getPageInnerHeight() throws Throwable {
        return Integer.parseInt(JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "window.innerHeight"));
    }

    private double getVisualViewportHeight() throws Throwable {
        return Float.parseFloat(JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "window.visualViewport.height"));
    }

    private void hideKeyboard() {
        JavaScriptUtils.executeJavaScript(
                getWebContents(), "document.querySelector('input').blur()");
    }

    private double getKeyboardHeightDp() {
        final double dpi = Coordinates.createFor(getWebContents()).getDeviceScaleFactor();
        double keyboardHeightPx = mActivityTestRule.getKeyboardDelegate().calculateKeyboardHeight(
                mActivityTestRule.getActivity().getWindow().getDecorView().getRootView());
        return keyboardHeightPx / dpi;
    }

    /**
     * Tests the default behavior of the virtual keyboard which is to resize the layout viewport and
     * initial containing block of the page.
     */
    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.OSK_RESIZES_VISUAL_VIEWPORT})
    public void testVirtualKeyboardDefaultResizeMode() throws Throwable {
        startMainActivityWithURL("/chrome/test/data/android/page_with_editable.html");
        int initialHeight = getPageInnerHeight();
        double initialVVHeight = getVisualViewportHeight();

        DOMUtils.clickNode(getWebContents(), TEXTFIELD_DOM_ID);
        assertWaitForKeyboardStatus(true);

        double keyboardHeight = getKeyboardHeightDp();

        // Use less than or equal since the keyboard may actually include accessories like the
        // Autofill bar. +1px delta to account for device scale factor rounding.
        assertWaitForPageHeight(lessThanOrEqualTo((int) (initialHeight - keyboardHeight + 1.0)));
        assertWaitForVisualViewportHeight(
                lessThanOrEqualTo(initialVVHeight - keyboardHeight + 1.0));

        // Hide the OSK and ensure the state is correctly restored to the initial height.
        hideKeyboard();
        assertWaitForKeyboardStatus(false);

        assertWaitForPageHeight(Matchers.is(initialHeight));
        assertWaitForVisualViewportHeight(
                Matchers.closeTo((double) initialVVHeight, /*error=*/1.0));
    }

    /**
     * Tests the OSKResizesVisualViewport flag changes Chrome's default behavior to the virtual
     * keyboard resizing only the visual viewport, but not the page's initial containing block or
     * layout viewport.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.OSK_RESIZES_VISUAL_VIEWPORT})
    public void testVirtualKeyboardResizesVisualViewportFlag() throws Throwable {
        startMainActivityWithURL("/chrome/test/data/android/page_with_editable.html");

        int initialHeight = getPageInnerHeight();
        double initialVVHeight = getVisualViewportHeight();

        DOMUtils.clickNode(getWebContents(), TEXTFIELD_DOM_ID);
        assertWaitForKeyboardStatus(true);

        double keyboardHeight = getKeyboardHeightDp();

        // Use less than or equal since the keyboard may actually include accessories like the
        // Autofill bar. +1 to account for device scale factor rounding.
        assertWaitForVisualViewportHeight(lessThanOrEqualTo(initialVVHeight - keyboardHeight + 1));
        assertWaitForPageHeight(Matchers.is(initialHeight));

        // Hide the OSK and ensure the state is correctly restored to the initial height.
        hideKeyboard();
        assertWaitForKeyboardStatus(false);

        assertWaitForPageHeight(Matchers.is(initialHeight));
        assertWaitForVisualViewportHeight(
                Matchers.closeTo((double) initialVVHeight, /*error=*/1.0));
    }

    /**
     * Tests the <meta name="viewport" content="virtual-keyboard=resize-layout"> tag opts the page
     * back into a mode where the keyboard resizes layout.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.OSK_RESIZES_VISUAL_VIEWPORT})
    public void testResizeLayoutMetaTag() throws Throwable {
        startMainActivityWithURL("/chrome/test/data/android/page_with_editable.html?resize-layout");
        int initialHeight = getPageInnerHeight();
        double initialVVHeight = getVisualViewportHeight();

        DOMUtils.clickNode(getWebContents(), TEXTFIELD_DOM_ID);
        assertWaitForKeyboardStatus(true);

        double keyboardHeight = getKeyboardHeightDp();

        // Use less than or equal since the keyboard may actually include accessories like the
        // Autofill bar. +1px to account for device scale factor rounding.
        assertWaitForPageHeight(lessThanOrEqualTo((int) (initialHeight - keyboardHeight + 1.0)));
        assertWaitForVisualViewportHeight(
                lessThanOrEqualTo(initialVVHeight - keyboardHeight + 1.0));

        // Hide the OSK and ensure the state is correctly restored to the initial height.
        // InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        hideKeyboard();
        assertWaitForKeyboardStatus(false);

        assertWaitForPageHeight(Matchers.is(initialHeight));
        assertWaitForVisualViewportHeight(
                Matchers.closeTo((double) initialVVHeight, /*error=*/1.0));
    }
}
