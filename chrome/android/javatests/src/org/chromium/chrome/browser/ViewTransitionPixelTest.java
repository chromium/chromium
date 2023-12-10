// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.text.TextUtils;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.ui.test.util.RenderTestRule;

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
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "hide-scrollbars"})
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

    private int mInitialPageHeight;
    private double mInitialVVHeight;

    @VirtualKeyboardMode.EnumType
    private int mVirtualKeyboardMode = VirtualKeyboardMode.RESIZES_VISUAL;

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
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

        mInitialPageHeight = getPageInnerHeight();
        mInitialVVHeight = getVisualViewportHeight();
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

    private void assertWaitForPageHeight(double expectedPageHeight) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        int curHeight = getPageInnerHeight();
                        // Allow 1px delta to account for device scale factor rounding.
                        Criteria.checkThat(
                                (double) curHeight,
                                Matchers.closeTo(expectedPageHeight, /* error= */ 1.0));
                    } catch (Throwable e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                TEST_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void assertWaitForVisualViewportHeight(double expectedHeight) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        double curHeight = getVisualViewportHeight();
                        // Allow 1px delta to account for device scale factor rounding.
                        Criteria.checkThat(
                                curHeight, Matchers.closeTo(expectedHeight, /* error= */ 1.0));
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

    private int getPageInnerHeight() throws Throwable {
        return Integer.parseInt(
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        getWebContents(), "window.innerHeight"));
    }

    private double getVisualViewportHeight() throws Throwable {
        return Float.parseFloat(
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        getWebContents(), "window.visualViewport.height"));
    }

    private void showAndWaitForKeyboard() throws Throwable {
        DOMUtils.clickNode(getWebContents(), TEXTFIELD_DOM_ID);
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getManualFillingComponent()
                                .forceShowForTesting());

        assertWaitForKeyboardStatus(true);

        double keyboardHeight = getKeyboardHeightDp();

        if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_VISUAL) {
            assertWaitForVisualViewportHeight(mInitialVVHeight - keyboardHeight);
        } else if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_CONTENT) {
            assertWaitForPageHeight(mInitialPageHeight - keyboardHeight);
        } else {
            Assert.fail("Unimplemented keyboard mode");
        }
    }

    private void hideKeyboard() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getManualFillingComponent().hide());
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "document.activeElement.blur()");
    }

    private void waitForKeyboardHidden() {
        assertWaitForKeyboardStatus(false);
        if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_VISUAL) {
            assertWaitForVisualViewportHeight(mInitialVVHeight);
        } else if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_CONTENT) {
            assertWaitForPageHeight(mInitialPageHeight);
        } else {
            Assert.fail("Unimplemented keyboard mode");
        }
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

    private Bitmap takeScreenshot() throws Throwable {
        Context context = ApplicationProvider.getApplicationContext();

        final CallbackHelper ch = new CallbackHelper();
        final AtomicReference<String> screenshotOutputPath = new AtomicReference<>();
        String cacheDirPath = context.getCacheDir().getAbsolutePath();
        TestThreadUtils.runOnUiThreadBlocking(
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

    // Force generating a new compositor frame from the renderer and wait until
    // its presented on screen.
    private void waitForFramePresented() throws Throwable {
        final CallbackHelper ch = new CallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getWebContents()
                            .getMainFrame()
                            .insertVisualStateCallback(result -> ch.notifyCalled());
                });

        ch.waitForNext(TEST_TIMEOUT, TimeUnit.SECONDS);

        // insertVisualStateCallback replies when a CompositorFrame is submitted. However, we want
        // to wait until the Viz process has received the new CompositorFrame so that the new frame
        // is available to a CopySurfaceRequest. Waiting for a second frame to be submitted
        // guarantees this since it cannot be sent until the first frame was ACKed by Viz.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getWebContents()
                            .getMainFrame()
                            .insertVisualStateCallback(result -> ch.notifyCalled());
                });

        ch.waitForNext(TEST_TIMEOUT, TimeUnit.SECONDS);
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
    // TODO(crbug.com/1453741): Fix test with CREATE_NEW_TAB_INITIALIZE_RENDERER.
    @DisableFeatures(ChromeFeatureList.CREATE_NEW_TAB_INITIALIZE_RENDERER)
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
        waitForFramePresented();

        Bitmap oldState = takeScreenshot();
        mRenderTestRule.compareForResult(oldState, "old_state_keyboard_resizes_visual");

        animateToEndState();
        waitForFramePresented();

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
    // TODO(crbug.com/1453741): Fix test with CREATE_NEW_TAB_INITIALIZE_RENDERER.
    @DisableFeatures(ChromeFeatureList.CREATE_NEW_TAB_INITIALIZE_RENDERER)
    public void testVirtualKeyboardResizesContent() throws Throwable {
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
        waitForFramePresented();

        Bitmap oldState = takeScreenshot();
        mRenderTestRule.compareForResult(oldState, "old_state_keyboard_resizes_content");

        animateToEndState();
        waitForFramePresented();

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
    // TODO(crbug.com/1453741): Fix test with CREATE_NEW_TAB_INITIALIZE_RENDERER.
    @DisableFeatures(ChromeFeatureList.CREATE_NEW_TAB_INITIALIZE_RENDERER)
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
        waitForFramePresented();

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
    // TODO(crbug.com/1453741): Fix test with CREATE_NEW_TAB_INITIALIZE_RENDERER.
    @DisableFeatures(ChromeFeatureList.CREATE_NEW_TAB_INITIALIZE_RENDERER)
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
        waitForFramePresented();

        Bitmap newState = takeScreenshot();
        mRenderTestRule.compareForResult(newState, "wider-than-icb");

        finishAnimations();
    }
}
