// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.hamcrest.Matchers.lessThanOrEqualTo;

import android.util.JsonReader;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.ui.mojom.VirtualKeyboardMode;

import java.io.IOException;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.concurrent.TimeUnit;

/** Tests the virtual keyboard's effect on resizing web pages. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
@Batch(Batch.PER_CLASS)
public class VirtualKeyboardResizeTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String TEXTFIELD_DOM_ID = "inputElement";
    private static final int TEST_TIMEOUT = 10000;

    private static PrefService getPrefService() {
        return UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
    }

    @After
    public void tearDown() {
        // Some tests set this pref. Clear it to ensure that state does not leak between tests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().clearPref(Pref.VIRTUAL_KEYBOARD_RESIZES_LAYOUT_BY_DEFAULT);
                });
    }

    private void startMainActivityWithURL(String url) throws Throwable {
        mActivityTestRule.startOnTestServerUrl(url);
        mActivityTestRule.getActivityTestRule().waitForActivityNativeInitializationComplete();

        // Ensure a compositor commit has occurred. This ensures that browser
        // controls shown state is synced to Blink before we start querying
        // visual viewport geometry.
        waitForVisualStateCallback();
    }

    private void waitForVisualStateCallback() throws Throwable {
        final CallbackHelper ch = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getWebContents()
                            .getMainFrame()
                            .insertVisualStateCallback(result -> ch.notifyCalled());
                });

        ch.waitForNext(TEST_TIMEOUT, TimeUnit.SECONDS);
    }

    private void navigateToURL(String url) {
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(url));
    }

    private void openInNewTab(String url) {
        mActivityTestRule.loadUrlInNewTab(mActivityTestRule.getTestServer().getURL(url));
    }

    private void assertWaitForKeyboardStatus(final boolean show) {
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean isKeyboardShowing =
                            mActivityTestRule
                                    .getKeyboardDelegate()
                                    .isKeyboardShowing(
                                            mActivityTestRule.getActivity(),
                                            mActivityTestRule.getActivity().getTabsView());
                    Criteria.checkThat(isKeyboardShowing, Matchers.is(show));
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void assertWaitForPageHeight(Matcher<java.lang.Integer> matcher) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        int curHeight = getPageInnerHeight();
                        Criteria.checkThat(curHeight, matcher);
                    } catch (Throwable e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void assertWaitForVisualViewportHeight(Matcher<java.lang.Double> matcher) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        double curHeight = getVisualViewportHeight();
                        Criteria.checkThat(curHeight, matcher);
                    } catch (Throwable e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void assertWaitForNthGeometryChangeEvent(final int n) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        int numGeometryChangeEvents = getNumGeometryChangeEvents();
                        Criteria.checkThat(numGeometryChangeEvents, greaterThanOrEqualTo(n));
                    } catch (Throwable e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private WebContents getWebContents() {
        return mActivityTestRule.getActivity().getActivityTab().getWebContents();
    }

    private int getNumGeometryChangeEvents() throws Throwable {
        return Integer.parseInt(
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        getWebContents(), "window.numGeometryChangeEvents"));
    }

    private int getPageInnerHeight() throws Throwable {
        return Integer.parseInt(
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        getWebContents(), "window.innerHeight"));
    }

    private ArrayList<Integer> getResizeEventLog() throws Throwable {
        String jsonText =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        getWebContents(), "window.resizeEventLog");
        JsonReader jsonReader = new JsonReader(new StringReader(jsonText));
        ArrayList<Integer> pageHeights = new ArrayList<Integer>();
        try {
            jsonReader.beginArray();
            while (jsonReader.hasNext()) {
                pageHeights.add(jsonReader.nextInt());
            }
            jsonReader.endArray();

            jsonReader.close();
        } catch (IOException exception) {
            Assert.fail("Failed to evaluate JavaScript: " + jsonText + "\n" + exception);
        }

        return pageHeights;
    }

    private void clearResizeEventLog() throws Throwable {
        JavaScriptUtils.executeJavaScript(getWebContents(), "window.resizeEventLog = []");
    }

    private double getVisualViewportHeight() throws Throwable {
        return Float.parseFloat(
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        getWebContents(), "window.visualViewport.height"));
    }

    private void hideKeyboard() {
        JavaScriptUtils.executeJavaScript(
                getWebContents(), "document.querySelector('input').blur()");
    }

    private double getKeyboardHeightDp() {
        final double dpi = Coordinates.createFor(getWebContents()).getDeviceScaleFactor();
        double keyboardHeightPx =
                mActivityTestRule
                        .getKeyboardDelegate()
                        .calculateTotalKeyboardHeight(
                                mActivityTestRule
                                        .getActivity()
                                        .getWindow()
                                        .getDecorView()
                                        .getRootView());
        return keyboardHeightPx / dpi;
    }

    /**
     * This is the same as testVirtualKeyboardDefaultResizeMode, except that the
     * OSKResizesVisualViewportByDefault flag is enabled. Normally this would cause the default
     * resize behavior to be "resize-visual", but since the
     * VIRTUAL_KEYBOARD_RESIZES_LAYOUT_BY_DEFAULT is set to "true", the default resize behavior
     * should ultimately be forced to "resize-content".
     */
    @Test
    @MediumTest
    public void testVirtualKeyboardDefaultResizeModeWithPref() throws Throwable {
        startMainActivityWithURL("/chrome/test/data/android/about.html");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.VIRTUAL_KEYBOARD_RESIZES_LAYOUT_BY_DEFAULT, true);
                });

        // Load the page after changing the pref.
        navigateToURL("/chrome/test/data/android/page_with_editable.html");
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
                Matchers.closeTo((double) initialVVHeight, /* error= */ 1.0));
    }

    /**
     * The same as the previous test, but sets the VirtualKeyboardResizesLayoutByDefault policy
     * rather than the pref directly.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"policy={\"VirtualKeyboardResizesLayoutByDefault\":true}"})
    @DisabledTest(message = "crbug.com/353947757")
    public void testVirtualKeyboardDefaultResizeModeWithPolicy() throws Throwable {
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
                Matchers.closeTo((double) initialVVHeight, /* error= */ 1.0));
    }

    /**
     * Tests the OSKResizesVisualViewportByDefault flag changes Chrome's default behavior to the
     * virtual keyboard resizing only the visual viewport, but not the page's initial containing
     * block or layout viewport.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/355432932")
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
                Matchers.closeTo((double) initialVVHeight, /* error= */ 1.0));
    }

    /**
     * Tests the <meta name="viewport" content="interactive-widget=resizes-visual"> tag causes the
     * page to resize only the visual viewport.
     */
    @Test
    @MediumTest
    public void testResizesVisualMetaTag() throws Throwable {
        startMainActivityWithURL("/chrome/test/data/android/about.html");

        // Setting the pref should have no effect on the result, since the <meta> tag explicitly
        // sets a *non-default* OSK resize behavior.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.VIRTUAL_KEYBOARD_RESIZES_LAYOUT_BY_DEFAULT, true);
                });

        // Load the page after changing the pref.
        navigateToURL("/chrome/test/data/android/page_with_editable.html?resizes-visual");

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
                Matchers.closeTo((double) initialVVHeight, /* error= */ 1.0));
    }

    /**
     * Tests the <meta name="viewport" content="interactive-widget=resizes-content"> tag opts the
     * page back into a mode where the keyboard resizes layout.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/383769050")
    public void testResizesLayoutMetaTag() throws Throwable {
        startMainActivityWithURL(
                "/chrome/test/data/android/page_with_editable.html?resizes-content");
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
        hideKeyboard();
        assertWaitForKeyboardStatus(false);

        assertWaitForPageHeight(Matchers.is(initialHeight));
        assertWaitForVisualViewportHeight(
                Matchers.closeTo((double) initialVVHeight, /* error= */ 1.0));
    }

    /**
     * Tests the <meta name="viewport" content="interactive-widget=overlays-content"> tag causes the
     * page to avoid resizing any viewports.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/351982700")
    public void testOverlaysContentMetaTag() throws Throwable {
        startMainActivityWithURL(
                "/chrome/test/data/android/page_with_editable.html?overlays-content");

        Assert.assertEquals(0, getNumGeometryChangeEvents());

        int initialHeight = getPageInnerHeight();
        double initialVVHeight = getVisualViewportHeight();

        DOMUtils.clickNode(getWebContents(), TEXTFIELD_DOM_ID);
        assertWaitForKeyboardStatus(true);
        assertWaitForNthGeometryChangeEvent(1);

        // We're checking something didn't happen so we have nothing to wait on - give it some time
        // to make sure a resize should often occur by now.
        Thread.sleep(200);

        // Ensure neither the innerHeight nor visualViewport height has changed.
        Assert.assertEquals(getPageInnerHeight(), initialHeight);
        Assert.assertEquals(getVisualViewportHeight(), initialVVHeight, /* delta= */ 1.0f);
    }

    /** Test that the virtual keyboard mode is correctly set/reset on navigations. */
    @Test
    @MediumTest
    public void testModeAfterNavigation() throws Throwable {
        startMainActivityWithURL("/chrome/test/data/android/page_with_editable.html");

        Assert.assertEquals(
                VirtualKeyboardMode.RESIZES_VISUAL,
                mActivityTestRule
                        .getActivity()
                        .getCompositorViewHolderForTesting()
                        .getVirtualKeyboardModeForTesting());

        navigateToURL("/chrome/test/data/android/page_with_editable.html?resizes-content");
        Assert.assertEquals(
                VirtualKeyboardMode.RESIZES_CONTENT,
                mActivityTestRule
                        .getActivity()
                        .getCompositorViewHolderForTesting()
                        .getVirtualKeyboardModeForTesting());

        navigateToURL("/chrome/test/data/android/page_with_editable.html");

        Assert.assertEquals(
                VirtualKeyboardMode.RESIZES_VISUAL,
                mActivityTestRule
                        .getActivity()
                        .getCompositorViewHolderForTesting()
                        .getVirtualKeyboardModeForTesting());

        navigateToURL("/chrome/test/data/android/page_with_editable.html?overlays-content");

        Assert.assertEquals(
                VirtualKeyboardMode.OVERLAYS_CONTENT,
                mActivityTestRule
                        .getActivity()
                        .getCompositorViewHolderForTesting()
                        .getVirtualKeyboardModeForTesting());

        openInNewTab("/chrome/test/data/android/page_with_editable.html?resizes-content");
        Assert.assertEquals(
                VirtualKeyboardMode.RESIZES_CONTENT,
                mActivityTestRule
                        .getActivity()
                        .getCompositorViewHolderForTesting()
                        .getVirtualKeyboardModeForTesting());

        // Ensure showing the keyboard and going through the resize flow uses the current virtual
        // keyboard mode.
        {
            int initialHeight = getPageInnerHeight();

            DOMUtils.clickNode(getWebContents(), TEXTFIELD_DOM_ID);
            assertWaitForKeyboardStatus(true);

            double keyboardHeight = getKeyboardHeightDp();

            assertWaitForPageHeight(
                    lessThanOrEqualTo((int) (initialHeight - keyboardHeight + 1.0)));

            hideKeyboard();
            assertWaitForKeyboardStatus(false);

            assertWaitForPageHeight(Matchers.is(initialHeight));
        }
    }

    /**
     * Test that the virtual keyboard mode is affected by the
     * VIRTUAL_KEYBOARD_RESIZES_LAYOUT_BY_DEFAULT pref on navigations.
     */
    @Test
    @MediumTest
    public void testModeAfterNavigationWithPref() throws Throwable {
        startMainActivityWithURL("/chrome/test/data/android/page_with_editable.html");

        Assert.assertEquals(
                VirtualKeyboardMode.RESIZES_VISUAL,
                mActivityTestRule
                        .getActivity()
                        .getCompositorViewHolderForTesting()
                        .getVirtualKeyboardModeForTesting());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.VIRTUAL_KEYBOARD_RESIZES_LAYOUT_BY_DEFAULT, true);
                });

        navigateToURL("/chrome/test/data/android/page_with_editable.html");
        Assert.assertEquals(
                VirtualKeyboardMode.RESIZES_CONTENT,
                mActivityTestRule
                        .getActivity()
                        .getCompositorViewHolderForTesting()
                        .getVirtualKeyboardModeForTesting());

        navigateToURL("/chrome/test/data/android/page_with_editable.html?overlays-content");
        Assert.assertEquals(
                VirtualKeyboardMode.OVERLAYS_CONTENT,
                mActivityTestRule
                        .getActivity()
                        .getCompositorViewHolderForTesting()
                        .getVirtualKeyboardModeForTesting());

        openInNewTab("/chrome/test/data/android/page_with_editable.html");
        Assert.assertEquals(
                VirtualKeyboardMode.RESIZES_CONTENT,
                mActivityTestRule
                        .getActivity()
                        .getCompositorViewHolderForTesting()
                        .getVirtualKeyboardModeForTesting());
    }

    /** Test that in overlays-content mode, the keyboard doesn't cause any transient resizes. */
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
    @DisabledTest(message = "crbug.com/414804967")
    public void testNoSpuriousResizeEventOverlaysContent() throws Throwable {
        startMainActivityWithURL(
                "/chrome/test/data/android/page_with_editable.html?overlays-content");
        clearResizeEventLog();

        int initialHeight = getPageInnerHeight();

        Assert.assertEquals(0, getNumGeometryChangeEvents());
        DOMUtils.clickNode(getWebContents(), TEXTFIELD_DOM_ID);
        assertWaitForNthGeometryChangeEvent(1);

        waitForVisualStateCallback();

        hideKeyboard();
        assertWaitForNthGeometryChangeEvent(2);

        waitForVisualStateCallback();

        // TODO(crbug.com/40822136): Ideally we'd check that we didn't get *any* resize event since
        // the page height isn't changing.  However, we inconsistently receive spurious resizes
        // during page load on Android. Until that's fixed, just ensure the page height is
        // consistent at each fired resize.
        ArrayList<Integer> pageHeights = getResizeEventLog();
        for (Integer pageHeight : pageHeights) {
            Assert.assertEquals(initialHeight, pageHeight.intValue());
        }
    }

    /** Test that in resizes-visual mode, the keyboard doesn't cause any transient resizes. */
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
    @DisabledTest(message = "crbug.com/414804967")
    public void testNoSpuriousResizeEventResizesVisual() throws Throwable {
        startMainActivityWithURL(
                "/chrome/test/data/android/page_with_editable.html?resizes-visual");
        clearResizeEventLog();

        int initialHeight = getPageInnerHeight();
        double initialVVHeight = getVisualViewportHeight();

        DOMUtils.clickNode(getWebContents(), TEXTFIELD_DOM_ID);
        assertWaitForKeyboardStatus(true);

        double keyboardHeight = getKeyboardHeightDp();

        // Use less than or equal since the keyboard may actually include accessories like the
        // Autofill bar. +1 to account for device scale factor rounding.
        assertWaitForVisualViewportHeight(lessThanOrEqualTo(initialVVHeight - keyboardHeight + 1));

        waitForVisualStateCallback();

        hideKeyboard();
        assertWaitForVisualViewportHeight(
                Matchers.closeTo((double) initialVVHeight, /* error= */ 1.0));

        waitForVisualStateCallback();

        // TODO(crbug.com/40822136): Ideally we'd check that we didn't get *any* resize event since
        // the page height isn't changing.  However, we inconsistently receive spurious resizes
        // during page load on Android. Additionally, visual viewport (and browser controls) updates
        // will also induce resize events (see the TODO in WebViewImpl::ResizeWithBrowserControls).
        // Until these issues are fixed, just ensure the page height is consistent at each fired
        // resize.
        ArrayList<Integer> pageHeights = getResizeEventLog();
        for (Integer pageHeight : pageHeights) {
            Assert.assertEquals(initialHeight, pageHeight.intValue());
        }
    }
}
