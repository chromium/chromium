// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.text.TextUtils;
import android.util.Size;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.util.WindowInsetsUtils;

import java.io.File;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests view-transitions API in relation to Android Chrome UI.
 *
 * <p>These test that view transitions result in correctly sized and positioned transitions in the
 * presence/absence of UI such as virtual-keyboard and moveable URL bar.
 *
 * <p>See https://www.w3.org/TR/css-view-transitions-1/
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=ViewTransitionOnNavigation",
    // Resampling can make scroll offsets non-deterministic so turn it off to ensure hiding browser
    // controls works reliably;
    // Disable edge to edge as part of the test is measuring the keyboard's height, which differs
    // depending on whether Chrome is drawn e2e.
    "disable-features=ResamplingScrollEvents,DrawCutoutEdgeToEdge,EdgeToEdgeBottomChin",
    "hide-scrollbars"
})
@Batch(Batch.PER_CLASS)
public class ViewTransitionPixelTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.BLINK_VIEW_TRANSITIONS)
                    .build();

    private static final String TEXTFIELD_DOM_ID = "inputElement";
    private static final int TEST_TIMEOUT = 10000;

    private EmbeddedTestServer mTestServer;

    private ViewportTestUtils mViewportTestUtils;

    private int mInitialPageHeight;
    private double mInitialVVHeight;

    @VirtualKeyboardMode.EnumType
    private int mVirtualKeyboardMode = VirtualKeyboardMode.RESIZES_VISUAL;

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mViewportTestUtils = new ViewportTestUtils(mActivityTestRule);
        mViewportTestUtils.setUpForBrowserControls();
    }

    private void startKeyboardTest(@VirtualKeyboardMode.EnumType int vkMode) throws Throwable {
        mVirtualKeyboardMode = vkMode;
        String url = "/chrome/test/data/android/view_transition.html";

        if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_VISUAL) {
            url += "?resizes-visual";
        } else if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_CONTENT) {
            url += "?resizes-content";
        } else {
            Assert.fail("Unexpected virtual keyboard mode");
        }

        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(url));
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        mInitialPageHeight = mViewportTestUtils.getPageInnerHeightPx();
        mInitialVVHeight = mViewportTestUtils.getVisualViewportHeightPx();
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

    private WebContents getWebContents() {
        return mActivityTestRule.getActivity().getActivityTab().getWebContents();
    }

    private void showAndWaitForKeyboard() throws Throwable {
        DOMUtils.clickNode(getWebContents(), TEXTFIELD_DOM_ID);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getManualFillingComponent()
                                .forceShowForTesting());

        assertWaitForKeyboardStatus(true);

        double keyboardHeight = getKeyboardHeightDp();

        if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_VISUAL) {
            mViewportTestUtils.waitForExpectedVisualViewportHeight(
                    mInitialVVHeight - keyboardHeight);
        } else if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_CONTENT) {
            mViewportTestUtils.waitForExpectedPageHeight(mInitialPageHeight - keyboardHeight);
        } else {
            Assert.fail("Unimplemented keyboard mode");
        }
    }

    private void hideKeyboard() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getManualFillingComponent().hide());
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "document.activeElement.blur()");
    }

    private void waitForKeyboardHidden() {
        assertWaitForKeyboardStatus(false);
        if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_VISUAL) {
            mViewportTestUtils.waitForExpectedVisualViewportHeight(mInitialVVHeight);
        } else if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_CONTENT) {
            mViewportTestUtils.waitForExpectedPageHeight(mInitialPageHeight);
        } else {
            Assert.fail("Unimplemented keyboard mode");
        }
    }

    private double getKeyboardHeightDp() {
        double keyboardHeightPx =
                mActivityTestRule
                        .getKeyboardDelegate()
                        .calculateTotalKeyboardHeight(
                                mActivityTestRule
                                        .getActivity()
                                        .getWindow()
                                        .getDecorView()
                                        .getRootView());
        return keyboardHeightPx / mViewportTestUtils.getDeviceScaleFactor();
    }

    private void setLocationAndWaitForLoad(String url) {
        ChromeTabUtils.waitForTabPageLoaded(
                mActivityTestRule.getActivity().getActivityTab(),
                url,
                () -> {
                    try {
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                getWebContents(), "location = '" + url + "'");
                    } catch (Throwable e) {
                    }
                },
                /* secondsToWait= */ 10);
    }

    private Bitmap takeScreenshot() throws Throwable {
        Context context = ApplicationProvider.getApplicationContext();

        final CallbackHelper ch = new CallbackHelper();
        final AtomicReference<String> screenshotOutputPath = new AtomicReference<>();
        String cacheDirPath = context.getCacheDir().getAbsolutePath();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getWebContents()
                            .getRenderWidgetHostView()
                            .writeContentBitmapToDiskAsync(
                                    /* width= */ 0,
                                    /* height= */ 0,
                                    cacheDirPath,
                                    path -> {
                                        screenshotOutputPath.set(path);
                                        ch.notifyCalled();
                                    });
                });

        ch.waitForNext(TEST_TIMEOUT, TimeUnit.SECONDS);

        if (TextUtils.isEmpty(screenshotOutputPath.get())) {
            throw new Exception("Failed to take screenshot");
        }

        File outputFile = new File(screenshotOutputPath.get());
        outputFile.deleteOnExit();
        Bitmap screenshot = BitmapFactory.decodeFile(outputFile.getAbsolutePath());
        outputFile.delete();
        return screenshot;
    }

    // This will start the transition and wait until snapshots for the old page have been
    // taken. This will return only once the domUpdate callback has run (but not resolved). The
    // transition animation doesn't start until the test calls startTransitionAnimation.
    private void createTransitionAndWaitUntilDomUpdateDispatched() throws Throwable {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(), "createTransition()");
        JavaScriptUtils.runJavascriptWithAsyncResult(
                getWebContents(),
                "readyToStartPromise.then(() => domAutomationController.send(true));");
    }

    private void waitForTransitionReady() throws Throwable {
        JavaScriptUtils.runJavascriptWithAsyncResult(
                getWebContents(),
                "transition.ready.then(() => domAutomationController.send(true));");
    }

    // After calling createTransitionAndWaitUntilDomUpdateDispatched to create a transition, this
    // will continue the transition and start animating. Animations are immediately paused in
    // the initial "outgoing" state.
    private void startTransitionAnimation() throws Throwable {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "startTransitionAnimation();");
    }

    // Sets animation times to the final "incoming" state. Animations remain paused.
    private void animateToEndState() throws Throwable {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(), "animateToEndState();");
    }

    // Finishes all animations which also finishes the view transition.
    private void finishAnimations() throws Throwable {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(), "finishAnimations();");
    }

    private String getCurrentUrl() {
        return ChromeTabUtils.getUrlStringOnUiThread(
                mActivityTestRule.getActivity().getActivityTab());
    }

    /**
     * Test view transitions when going from a state with a virtual keyboard shown to the virtual
     * keyboard hidden.
     *
     * <p>This tests the default mode where the virtual-keyboard resizes only the visual viewport
     * and the author hasn't opted into a content-resizing virtual keyboard.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testVirtualKeyboardResizesVisual() throws Throwable {
        startKeyboardTest(VirtualKeyboardMode.RESIZES_VISUAL);

        showAndWaitForKeyboard();

        createTransitionAndWaitUntilDomUpdateDispatched();

        // Note: rendering is blocked here so we can't wait for the keyboard to hide since the size
        // depends on the renderer.
        hideKeyboard();

        // Start the animation. The "animation" simply displays the old transition for the full
        // duration of the test.
        startTransitionAnimation();

        waitForKeyboardHidden();

        // Wait for a frame to be presented to ensure the animation has started and the updated
        // viewport size is rendered.
        mViewportTestUtils.waitForFramePresented();

        Bitmap oldState = takeScreenshot();
        mRenderTestRule.compareForResult(oldState, "old_state_keyboard_resizes_visual");

        animateToEndState();
        mViewportTestUtils.waitForFramePresented();

        Bitmap newState = takeScreenshot();
        mRenderTestRule.compareForResult(newState, "new_state_keyboard_resizes_visual");

        finishAnimations();
    }

    /**
     * Test view transitions when going from a state with a virtual keyboard shown to the virtual
     * keyboard hidden.
     *
     * <p>This tests the mode where the author opts in to the keyboard resizing page content.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testVirtualKeyboardResizesContent() throws Throwable {
        doTestVirtualKeyboardResizesContent();
    }

    /**
     * Same as {@code #testVirtualKeyboardResizesContent()}, but with TabStripLayoutOptimization
     * enabled. This tablet feature uses caption bar insets to draw custom app headers and is known
     * to have caused regressions in bottom Chrome UI placement when OSK is visible.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @MinAndroidSdkLevel(VERSION_CODES.R)
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
    @Restriction(DeviceFormFactor.TABLET)
    public void testVirtualKeyboardResizesContent_TSLOEnabled() throws Throwable {
        // Simulate fullscreen window behavior in an environment that supports Android V custom app
        // header APIs.
        WindowInsetsUtils.setFrameForTesting(new Size(2560, 1600));
        WindowInsetsUtils.setWidestUnoccludedRectForTesting(new Rect());

        doTestVirtualKeyboardResizesContent();
    }

    private void doTestVirtualKeyboardResizesContent() throws Throwable {
        startKeyboardTest(VirtualKeyboardMode.RESIZES_CONTENT);

        showAndWaitForKeyboard();

        createTransitionAndWaitUntilDomUpdateDispatched();

        // Note: rendering is blocked here so we can't wait for the keyboard to hide since the size
        // of the renderer wont update until we start rendering again.
        hideKeyboard();

        // Start the animation. The "animation" simply displays the old transition for the full
        // duration of the test.
        startTransitionAnimation();

        waitForKeyboardHidden();

        // Wait for a frame to be presented to ensure the animation has started and the updated
        // viewport size is rendered.
        mViewportTestUtils.waitForFramePresented();

        Bitmap oldState = takeScreenshot();
        mRenderTestRule.compareForResult(oldState, "old_state_keyboard_resizes_content");

        animateToEndState();
        mViewportTestUtils.waitForFramePresented();

        Bitmap newState = takeScreenshot();
        mRenderTestRule.compareForResult(newState, "new_state_keyboard_resizes_content");

        finishAnimations();
    }

    /**
     * Test view transitions when into a <dialog> element.
     *
     * <p>Tested here to ensure snapshot viewport positioning behavior with respect to top-layer
     * objects like <dialog>.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testDialog() throws Throwable {
        String url = "/chrome/test/data/android/view_transition_dialog.html";
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(url));
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        createTransitionAndWaitUntilDomUpdateDispatched();

        // Start the animation. The "animation" simply displays the old transition for the full
        // duration of the test. This test is interested in how the <dialog> element is positioned.
        // Since that's in the end-state, skip straight to that.
        startTransitionAnimation();
        animateToEndState();
        mViewportTestUtils.waitForFramePresented();

        Bitmap newState = takeScreenshot();
        mRenderTestRule.compareForResult(newState, "incoming_dialog_element");

        finishAnimations();
    }

    /**
     * Test view transitions in a page wider than the initial containing block.
     *
     * <p>Tests that a view-transition fills the viewport when the fixed-containing-block is larger
     * than the initial-containing-block. This happens when an element on the page horizontally
     * overflows the initial-containing-block, increasing how much the page can be zoomed out.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPageWiderThanICB() throws Throwable {
        String url = "/chrome/test/data/android/view_transition_wider_than_icb.html";
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(url));
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        createTransitionAndWaitUntilDomUpdateDispatched();

        // Start the animation. The "animation" simply displays the old transition for the full
        // duration of the test. This test is interested in how the <dialog> element is positioned.
        // Since that's in the end-state, skip straight to that.
        startTransitionAnimation();
        animateToEndState();
        mViewportTestUtils.waitForFramePresented();

        Bitmap newState = takeScreenshot();
        mRenderTestRule.compareForResult(newState, "wider-than-icb");

        finishAnimations();
    }

    /**
     * Test the root snapshot is correctly displayed when browser controls overlay the page.
     *
     * <p>Perform a cross-document transition from a starting state where the browser controls are
     * hidden. The navigation will cause the browser controls to animate in, overlaying the content.
     * Ensure the root snapshot is correctly captured and displayed so that the old snapshot matches
     * the live incoming page.
     *
     * <p>The output screenshot should show blue strips (old snapshot) on the left and green strips
     * (new snapshot) on the right. The strips should line up exactly. Only the "OVERLAY" peach bar
     * should be visible and aligned with the viewport top edge.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBrowserControlsRootSnapshotControlsOverlay() throws Throwable {
        String url = "/chrome/test/data/android/view_transition_browser_controls.html";
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(url));
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        mViewportTestUtils.hideBrowserControls();

        // Scrolling to a non-0 y offset will cause controls to overlay content when they're shown.
        // Ensure we wait a frame before navigating so that the compositor receives the new scroll
        // offset before controls start to show.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "window.scrollTo(0, 1)");
        mViewportTestUtils.waitForFramePresented();
        setLocationAndWaitForLoad(getCurrentUrl() + "?next");

        mViewportTestUtils.waitForFramePresented();
        waitForTransitionReady();

        int oldPageScrollOffset = mViewportTestUtils.getTopControlsHeightDp() + 1;

        // Scroll the incoming page too just so the strips should line up.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "window.scrollTo(0, " + oldPageScrollOffset + ")");

        mViewportTestUtils.waitForFramePresented();
        mViewportTestUtils.waitForFramePresented();
        Bitmap newState = takeScreenshot();
        mRenderTestRule.compareForResult(newState, "browser-controls-overlay-root");

        finishAnimations();
    }

    /**
     * Test the root snapshot is correctly displayed when browser controls push the page.
     *
     * <p>Perform a cross-document transition from a starting state where the browser controls are
     * hidden. The navigation will cause the browser controls to animate in and push down the
     * content (as opposed to overlaying it). Ensure the root snapshot is correctly captured and
     * displayed so that the old snapshot matches the live incoming page.
     *
     * <p>The output screenshot should show blue strips (old snapshot) on the left and green strips
     * (new snapshot) on the right. The strips should line up exactly. Both peach bars should be
     * visible, with the "TOP" one at the viewport top edge.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBrowserControlsRootSnapshotControlsPush() throws Throwable {
        String url = "/chrome/test/data/android/view_transition_browser_controls.html";
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(url));
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        mViewportTestUtils.hideBrowserControls();

        // Ensure the page is at offset 0. When scrolled to the top the controls animation will push
        // the page down, rather than overlaying it. Ensure we wait a frame before navigating so
        // that the compositor receives the new scroll offset before controls start to show.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "window.scrollTo(0, 0)");
        mViewportTestUtils.waitForFramePresented();
        setLocationAndWaitForLoad(getCurrentUrl() + "?next");

        mViewportTestUtils.waitForFramePresented();
        waitForTransitionReady();

        mViewportTestUtils.waitForFramePresented();
        mViewportTestUtils.waitForFramePresented();
        Bitmap newState = takeScreenshot();
        mRenderTestRule.compareForResult(newState, "browser-controls-push-root");

        finishAnimations();
    }

    /**
     * Test child snapshots are correctly displayed when browser controls overlay the page.
     *
     * <p>Ensures child snapshots are correctly positioned on the new page when the browser controls
     * animation overlays page content. Tests for both in-flow (green box) and fixed (purple box)
     * children.
     *
     * <p>The output should show a green box exactly centered in a blue box. The purple bar at the
     * top should line up exactly with the black line on top of the "CONTROLS" peach bar.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBrowserControlsChildSnapshotControlsOverlay() throws Throwable {
        String url = "/chrome/test/data/android/view_transition_browser_controls_child.html";
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(url));
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        mViewportTestUtils.hideBrowserControls();

        // Scrolling to a non-0 y offset will cause controls to overlay content when they're shown.
        // Ensure we wait a frame before navigating so that the compositor receives the new scroll
        // offset before controls start to show.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "window.scrollTo(0, 1)");
        mViewportTestUtils.waitForFramePresented();
        setLocationAndWaitForLoad(getCurrentUrl() + "?next");

        mViewportTestUtils.waitForFramePresented();
        waitForTransitionReady();

        int oldPageScrollOffset = mViewportTestUtils.getTopControlsHeightDp() + 1;

        // Scroll the incoming page too just so the strips should line up.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "window.scrollTo(0, " + oldPageScrollOffset + ")");

        mViewportTestUtils.waitForFramePresented();
        mViewportTestUtils.waitForFramePresented();
        Bitmap newState = takeScreenshot();
        mRenderTestRule.compareForResult(newState, "browser-controls-overlay-child");

        finishAnimations();
    }

    /**
     * Test child snapshots are correctly displayed when browser controls push the page.
     *
     * <p>Ensures child snapshots are correctly positioned on the new page when the browser controls
     * animation pushes page content. Tests for both in-flow (green box) and fixed (purple box)
     * children.
     *
     * <p>The output should show a green box exactly centered in a blue box. There should be two
     * peach bars visible: "TOP" and "CONTROLS". The purple bar at the top should line up exactly
     * with the black line on top of the "TOP" peach bar.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBrowserControlsChildSnapshotControlsPush() throws Throwable {
        String url = "/chrome/test/data/android/view_transition_browser_controls_child.html";
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(url));
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        mViewportTestUtils.hideBrowserControls();

        // Ensure the page is at offset 0. When scrolled to the top the controls animation will push
        // the page down, rather than overlaying it. Ensure we wait a frame before navigating so
        // that the compositor receives the new scroll offset before controls start to show.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "window.scrollTo(0, 0)");
        mViewportTestUtils.waitForFramePresented();
        setLocationAndWaitForLoad(getCurrentUrl() + "?next");

        mViewportTestUtils.waitForFramePresented();
        waitForTransitionReady();

        mViewportTestUtils.waitForFramePresented();
        mViewportTestUtils.waitForFramePresented();
        Bitmap newState = takeScreenshot();
        mRenderTestRule.compareForResult(newState, "browser-controls-push-child");

        finishAnimations();
    }
}
