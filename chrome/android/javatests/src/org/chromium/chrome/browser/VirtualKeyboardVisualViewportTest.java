// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.view.View;

import androidx.core.view.WindowInsetsCompat;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.FullscreenManagerTestUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;

import java.util.Locale;

/**
 * End-to-end tests that the visual viewport ends at the top of the virtual keyboard, as the default
 * interactive-widget=resizes-visual mode promises. See https://crbug.com/565837684.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Keyboard and browser controls state must not leak between tests.")
public class VirtualKeyboardVisualViewportTest {
    private static final String TAG = "VKVisualViewport";
    private static final String INPUT_ID = "input";
    private static final long TIMEOUT_MS = 10000;
    private static final int MAX_SCROLL_ATTEMPTS = 5;

    // Tall enough for the browser controls to scroll away, with an input that stays on screen
    // after that scroll.
    private static final String PAGE =
            UrlUtils.encodeHtmlDataUri(
                    "<html><head><meta name='viewport' content='width=device-width,"
                            + " initial-scale=1'></head>"
                            + "<body style='margin: 0; height: 400vh'>"
                            + "<input id='input' autocomplete='off'"
                            + " style='position: absolute; top: 40vh; font-size: 20px'>"
                            + "</body></html>");

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                TabStateBrowserControlsVisibilityDelegate::disablePageLoadDelayForTests);
        FullscreenManagerTestUtils.disableBrowserOverrides();
    }

    /** The input is focused while the browser controls are showing. */
    @Test
    @MediumTest
    public void testControlsShown() throws Exception {
        focusInputWithControlsShown();
    }

    /**
     * The browser controls are scrolled away before the input is focused. Focusing an editable node
     * locks the controls shown, so they come back while the keyboard is showing. This is the flow
     * from https://crbug.com/565837684.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Large tablets lock the top controls.
    public void testControlsScrolledAwayFirst() throws Exception {
        focusInputAfterScrollingControlsAway();
    }

    // Control group: disabling the transient innerHeight fix is known to avoid the bug.

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.VIRTUAL_KEYBOARD_TRANSIENT_INNER_HEIGHT_FIX)
    public void testControlsShown_withoutTransientInnerHeightFix() throws Exception {
        focusInputWithControlsShown();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.VIRTUAL_KEYBOARD_TRANSIENT_INNER_HEIGHT_FIX)
    @Restriction(DeviceFormFactor.PHONE) // Large tablets lock the top controls.
    public void testControlsScrolledAwayFirst_withoutTransientInnerHeightFix() throws Exception {
        focusInputAfterScrollingControlsAway();
    }

    private void focusInputWithControlsShown() throws Exception {
        ChromeTabbedActivity activity = mActivityTestRule.startOnUrl(PAGE).getActivity();
        FullscreenManagerTestUtils.waitForBrowserControlsPosition(activity, 0);
        focusInput(activity);
        assertVisualViewportEndsAtKeyboardTop(activity);
    }

    private void focusInputAfterScrollingControlsAway() throws Exception {
        ChromeTabbedActivity activity = mActivityTestRule.startOnUrl(PAGE).getActivity();
        scrollBrowserControlsAway(activity);
        focusInput(activity);
        assertVisualViewportEndsAtKeyboardTop(activity);
    }

    /**
     * Drags the page up until the top controls have fully scrolled away.
     *
     * <p>With the browser controls scroll snap animation, the controls don't follow the scroll.
     * They animate away once the page has scrolled far enough, and can stop part way until the next
     * scroll. FullscreenManagerTestUtils expects a single drag to hide them (crbug.com/489060623),
     * so this drags again until they're hidden.
     */
    private static void scrollBrowserControlsAway(ChromeTabbedActivity activity) {
        FullscreenManagerTestUtils.waitForBrowserControlsPosition(activity, 0);
        Tab tab = ThreadUtils.runOnUiThreadBlocking(activity::getActivityTab);
        FullscreenManagerTestUtils.waitForPageToBeScrollable(tab);

        BrowserControlsStateProvider controls = activity.getBrowserControlsManager();
        int topControlsHeight = ThreadUtils.runOnUiThreadBlocking(controls::getTopControlsHeight);
        // Start above the bottom controls. Each drag scrolls the page by nearly twice the height of
        // the top controls, past the region at the top of the page where the snap animation won't
        // hide them.
        float fromY =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> tab.getView().getHeight() - controls.getBottomControlsHeight() - 50f);
        float toY = fromY - 2 * topControlsHeight;
        for (int attempt = 1; ; attempt++) {
            TouchCommon.performDragNoFling(
                    activity, 50f, 50f, fromY, toY, /* stepCount= */ 50, /* duration= */ 500);
            try {
                FullscreenManagerTestUtils.waitForBrowserControlsPosition(
                        activity, -topControlsHeight);
                Log.i(TAG, "Top controls scrolled away after %d drag(s).", attempt);
                return;
            } catch (CriteriaHelper.TimeoutException e) {
                if (attempt == MAX_SCROLL_ATTEMPTS) throw e;
            }
        }
    }

    private void focusInput(ChromeTabbedActivity activity) throws Exception {
        DOMUtils.clickNode(mActivityTestRule.getWebContents(), INPUT_ID);
        CriteriaHelper.pollUiThread(
                () ->
                        mActivityTestRule
                                .getKeyboardDelegate()
                                .isKeyboardObscuring(activity.getTabsViewForTesting()),
                TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        // Focusing an editable node locks the browser controls shown.
        FullscreenManagerTestUtils.waitForBrowserControlsPosition(activity, 0);
    }

    /**
     * Waits for the bottom of the visual viewport to line up with the top of the keyboard (or of
     * the keyboard accessory, if one is showing), within a CSS pixel.
     */
    private void assertVisualViewportEndsAtKeyboardTop(ChromeTabbedActivity activity) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    ViewportAndKeyboardState state;
                    try {
                        state = measure(activity);
                    } catch (Exception e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                    Criteria.checkThat(
                            "The keyboard isn't showing. " + state,
                            state.mImeBottomInset,
                            Matchers.greaterThan(0));
                    // visualViewport.height is in fractional CSS pixels, so converting it back to
                    // physical pixels may round off by one.
                    Criteria.checkThat(
                            state.toString(),
                            Math.abs(state.visualViewportDeltaFromKeyboard()),
                            Matchers.lessThanOrEqualTo(1));
                    Log.i(TAG, state.toString());
                },
                TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /** Window state of the visual viewport and the keyboard, in physical pixels. */
    private static class ViewportAndKeyboardState {
        float mDsf;
        double mVisualViewportHeight;
        int mContentTop;
        int mVisualViewportBottom;
        int mImeBottomInset;
        int mKeyboardTop;
        int mAccessoryHeight;
        int mNavigationBarHeight;
        int mBottomControlsHeight;

        /** Positive when the visual viewport extends behind the keyboard. */
        int visualViewportDeltaFromKeyboard() {
            return mVisualViewportBottom - (mKeyboardTop - mAccessoryHeight);
        }

        @Override
        public String toString() {
            return String.format(
                    Locale.US,
                    "Visual viewport bottom is %dpx (%.1fdp) %s the keyboard top. vvBottom=%d"
                            + " keyboardTop=%d accessory=%d contentTop=%d vv.height=%.1f"
                            + " dsf=%.3f navigationBar=%d bottomControls=%d",
                    Math.abs(visualViewportDeltaFromKeyboard()),
                    Math.abs(visualViewportDeltaFromKeyboard()) / mDsf,
                    visualViewportDeltaFromKeyboard() > 0 ? "below" : "above",
                    mVisualViewportBottom,
                    mKeyboardTop,
                    mAccessoryHeight,
                    mContentTop,
                    mVisualViewportHeight,
                    mDsf,
                    mNavigationBarHeight,
                    mBottomControlsHeight);
        }
    }

    private ViewportAndKeyboardState measure(ChromeTabbedActivity activity) throws Exception {
        WebContents webContents = mActivityTestRule.getWebContents();
        double visualViewportHeight =
                Double.parseDouble(
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                webContents, "visualViewport.height * visualViewport.scale"));
        float dsf = Coordinates.createFor(webContents).getDeviceScaleFactor();
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewportAndKeyboardState state = new ViewportAndKeyboardState();
                    state.mDsf = dsf;
                    state.mVisualViewportHeight = visualViewportHeight;

                    // The web content is drawn below the top controls, and the visual viewport
                    // starts where the web content starts.
                    int[] location = new int[2];
                    activity.getActivityTab().getContentView().getLocationInWindow(location);
                    state.mContentTop =
                            location[1] + activity.getBrowserControlsManager().getContentOffset();
                    state.mVisualViewportBottom =
                            state.mContentTop + (int) Math.round(visualViewportHeight * dsf);

                    // The IME inset covers the keyboard and, when drawing edge-to-edge, the
                    // navigation bar below it.
                    View root = activity.getWindow().getDecorView();
                    WindowInsetsCompat insets =
                            WindowInsetsCompat.toWindowInsetsCompat(
                                    root.getRootWindowInsets(), root);
                    state.mImeBottomInset = insets.getInsets(WindowInsetsCompat.Type.ime()).bottom;
                    state.mKeyboardTop = root.getHeight() - state.mImeBottomInset;
                    state.mNavigationBarHeight =
                            insets.getInsets(WindowInsetsCompat.Type.navigationBars()).bottom;

                    WindowAndroid window = activity.getWindowAndroid();
                    state.mAccessoryHeight =
                            window == null
                                    ? 0
                                    : window.getApplicationBottomInsetTracker()
                                            .getInsets()
                                            .viewVisibleHeightInset;
                    state.mBottomControlsHeight =
                            activity.getBrowserControlsManager().getBottomControlsHeight();
                    return state;
                });
    }
}
