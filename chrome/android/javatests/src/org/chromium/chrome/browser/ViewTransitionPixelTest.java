// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.hamcrest.Matchers.lessThanOrEqualTo;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Rect;
import android.support.test.InstrumentationRegistry;
import android.util.Base64;

import androidx.test.filters.MediumTest;
import androidx.test.uiautomator.UiDevice;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.mojom.VirtualKeyboardMode;

import java.io.ByteArrayOutputStream;
import java.io.File;

/**
 * Tests view-transitions API in relation to Android Chrome UI.
 *
 * These test that view transitions result in correctly sized and positioned transitions in the
 * presence/absence of UI such as virtual-keyboard and moveable URL bar.
 *
 * See https://github.com/WICG/shared-element-transitions/blob/main/explainer.md
 * TODO(bokan): Link to relevant spec when ready.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=ViewTransition"

})
@Batch(Batch.PER_CLASS)
public class ViewTransitionPixelTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEXTFIELD_DOM_ID = "inputElement";
    private static final int TEST_TIMEOUT = 10000;

    // How many pixels can differ in the pixel test before comparison is considered non-equivalent.
    // Experimentally determined to allow some difference due to compositing and raster but not to
    // allow false positives.
    private static final int PIXEL_DIFF_FUZZINESS = 4000;

    private static final String CAPTURE_NEW = "new";
    private static final String CAPTURE_OLD = "old";

    private EmbeddedTestServer mTestServer;

    private int mInitialPageHeight;
    private double mInitialVVHeight;

    @VirtualKeyboardMode.EnumType
    private int mVirtualKeyboardMode = VirtualKeyboardMode.RESIZES_VISUAL;

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    private void startTest(@VirtualKeyboardMode.EnumType int vkMode, String captureWhich)
            throws Throwable {
        mVirtualKeyboardMode = vkMode;
        String url = "/chrome/test/data/android/view_transition.html";

        if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_VISUAL) {
            url += "?resizes-visual";
        } else if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_CONTENT) {
            url += "?resizes-content";
        } else {
            Assert.fail("Unexpected virtual keyboard mode");
        }

        // Set the animation direction to either capture the new or the old state.
        url += "&" + captureWhich;

        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(url));
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        mInitialPageHeight = getPageInnerHeight();
        mInitialVVHeight = getVisualViewportHeight();

        // startMainActivityWithURL will wait for the renderer to produce a
        // frame but not for it to be presented. Since we can take screenshots
        // right after this, ensure we've presented.
        waitForFramePresented();
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

    private void showAndWaitForKeyboard() throws Throwable {
        DOMUtils.clickNode(getWebContents(), TEXTFIELD_DOM_ID);

        assertWaitForKeyboardStatus(true);

        double keyboardHeight = getKeyboardHeightDp();

        // Use less than or equal since the keyboard may actually include accessories like the
        // Autofill bar. +1px delta to account for device scale factor rounding.
        if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_VISUAL) {
            assertWaitForVisualViewportHeight(
                    lessThanOrEqualTo(mInitialVVHeight - keyboardHeight + 1));
        } else if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_CONTENT) {
            assertWaitForPageHeight(
                    lessThanOrEqualTo((int) (mInitialPageHeight - keyboardHeight + 1.0)));
        } else {
            Assert.fail("Unimplemented keyboard mode");
        }
    }

    private void hideKeyboard() throws Throwable {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "document.activeElement.blur()");
    }

    private void waitForKeyboardHidden() {
        assertWaitForKeyboardStatus(false);
        if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_VISUAL) {
            assertWaitForVisualViewportHeight(
                    Matchers.closeTo((double) mInitialVVHeight, /*error=*/1.0));
        } else if (mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_CONTENT) {
            assertWaitForPageHeight(Matchers.is(mInitialPageHeight));
        } else {
            Assert.fail("Unimplemented keyboard mode");
        }
    }

    private double getKeyboardHeightDp() {
        final double dpi = Coordinates.createFor(getWebContents()).getDeviceScaleFactor();
        double keyboardHeightPx = mActivityTestRule.getKeyboardDelegate().calculateKeyboardHeight(
                mActivityTestRule.getActivity().getWindow().getDecorView().getRootView());
        return keyboardHeightPx / dpi;
    }

    private String bitmapToDataUrl(Bitmap bmp) {
        // Scale down the image so that we can fit the resulting data: URL into Android's 4000
        // character log line limit. 300px of width is an arbitrary choice that experimentally
        // results in data: URLs about 1000 chars in length.
        double scale = 300.0 / bmp.getWidth();
        Bitmap scaledDownBmp = Bitmap.createScaledBitmap(
                bmp, (int) (bmp.getWidth() * scale), (int) (bmp.getHeight() * scale), true);

        ByteArrayOutputStream os = new ByteArrayOutputStream();
        scaledDownBmp.compress(Bitmap.CompressFormat.PNG, 50, os);
        String url =
                "data:image/png;base64," + Base64.encodeToString(os.toByteArray(), Base64.NO_WRAP);

        if (url.length() > 4000) {
            return "<IMAGE TOO LARGE TO FIT IN LOG>";
        }

        return url;
    }

    private Bitmap takeScreenshot() throws Throwable {
        Context context = InstrumentationRegistry.getContext();
        File outputFile = File.createTempFile("screenshot", "png", context.getCacheDir());
        outputFile.deleteOnExit();
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        if (!device.takeScreenshot(outputFile)) {
            throw new Exception("Failed to take screenshot");
        }

        // Clip so that only the content part of the snapshot remains.
        Rect clipRect = new Rect();
        mActivityTestRule.getActivity().getWindow().getDecorView().getWindowVisibleDisplayFrame(
                clipRect);

        float topControlsOffset = Coordinates.createFor(getWebContents()).getContentOffsetYPix();

        // Inset the rect from the top. Use set() rather than inset() since the latter is only
        // available in API level 31.
        clipRect.set(
                /*left=*/clipRect.left, /*top=*/clipRect.top + (int) Math.ceil(topControlsOffset),
                /*right=*/clipRect.right, /*bottom=*/clipRect.bottom);

        Bitmap fullScreenBmp = BitmapFactory.decodeFile(outputFile.getAbsolutePath());
        outputFile.delete();

        return Bitmap.createBitmap(
                fullScreenBmp, clipRect.left, clipRect.top, clipRect.width(), clipRect.height());
    }

    // This will start the transition and wait until snapshots for the old page have been
    // taken. This will return only once the domUpdate callback has run (but not resolved). The
    // transition animation doesn't start until the test calls startTransitionAnimation.
    private void createTransitionAndWaitUntilDomUpdateDispatched() throws Throwable {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(), "createTransition()");
        JavaScriptUtils.runJavascriptWithAsyncResult(getWebContents(),
                "readyToStartPromise.then(() => domAutomationController.send(true));");
    }

    // After calling createTransitionAndWaitUntilDomUpdateDispatched to create a transition, this
    // will continue the transition and start the animation.
    private void startTransitionAnimation() throws Throwable {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getWebContents(), "startTransitionAnimation();");
    }

    // Force generating a new compositor frame from the renderer and wait until
    // its presented on screen.
    private void waitForFramePresented() throws Throwable {
        final CallbackHelper ch = new CallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getWebContents().getMainFrame().forceRedrawAndWaitForPresentation(
                    () -> { ch.notifyCalled(); });
        });

        ch.waitForNext();

        // TODO(bokan): This seems to only show the expected changes after waiting for a second
        // frame. Investigate why. https://crbug.com/1350910.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getWebContents().getMainFrame().forceRedrawAndWaitForPresentation(
                    () -> { ch.notifyCalled(); });
        });

        ch.waitForNext();
    }

    // Compares the expected to the actual screenshot bitmaps and returns true if they're
    // equivalent. Some fuzziness is allowed to account for blending and compositing artifacts. If
    // failed, the test is failed with a message containing paths to a data: URL for each of the
    // bitmaps. These can be easily viewed using `adb shell cat filename` and copied into a browser
    // for viewing.
    private boolean doPixelTestScreenshotsMatch(Bitmap expected, Bitmap actual) throws Throwable {
        Assert.assertEquals(expected.getWidth(), actual.getWidth());
        Assert.assertEquals(expected.getHeight(), actual.getHeight());

        int failingPixels = 0;
        for (int y = 0; y < expected.getHeight(); ++y) {
            for (int x = 0; x < expected.getWidth(); ++x) {
                if (expected.getPixel(x, y) != actual.getPixel(x, y)) ++failingPixels;
            }
        }

        if (failingPixels > PIXEL_DIFF_FUZZINESS) {
            String url_expected = bitmapToDataUrl(expected);
            String url_actual = bitmapToDataUrl(actual);
            Assert.fail("Does not match reference screenshot, " + failingPixels
                    + " pixels differed.\n"
                    + " Expected screenshot (" + url_expected + ")\n"
                    + " Actual screenshot(" + url_actual + ")");
            return false;
        }

        return true;
    }

    /**
     * Test view transitions when going from a state with a virtual keyboard shown to the virtual
     * keyboard hidden.
     *
     * This tests the default mode where the virtual-keyboard resizes only the visual viewport and
     * the author hasn't opted into a content-resizing virtual keyboard.
     */
    @Test
    @MediumTest
    public void testVirtualKeyboardResizesVisualOld() throws Throwable {
        startTest(VirtualKeyboardMode.RESIZES_VISUAL, CAPTURE_OLD);

        // Since the keyboard affects only the visual viewport, the initial page is how we expect
        // the transition snapshot to look when the keyboard is hidden.
        Bitmap expected = takeScreenshot();

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

        Bitmap actual = takeScreenshot();

        Assert.assertTrue(doPixelTestScreenshotsMatch(expected, actual));
    }

    /**
     * Test view transitions when going from a state with a virtual keyboard shown to the virtual
     * keyboard hidden.
     *
     * This tests the mode where the author opts in to the keyboard resizing page content.
     */
    @Test
    @MediumTest
    public void testVirtualKeyboardResizesContentOld() throws Throwable {
        startTest(VirtualKeyboardMode.RESIZES_CONTENT, CAPTURE_OLD);

        // Since the keyboard affects content it'll push the bottom-fixed element up. Push it up
        // manually to take the expected screenshot.
        {
            showAndWaitForKeyboard();

            // Compare page innerHeight rather than getting keyboardHeight to also include any
            // keyboard accessories.
            int keyboardHeight = mInitialPageHeight - getPageInnerHeight();

            hideKeyboard();
            waitForKeyboardHidden();

            JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(),
                    "document.getElementById('bottomfixed').style.transform ="
                            + "'translateY(-" + keyboardHeight + "px)';");

            waitForFramePresented();
        }

        Bitmap expected = takeScreenshot();

        {
            JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(),
                    "document.getElementById('bottomfixed').style.transform = '';");
        }

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

        Bitmap actual = takeScreenshot();

        Assert.assertTrue(doPixelTestScreenshotsMatch(expected, actual));
    }

    /**
     * Test view transitions when going from a state with a virtual keyboard shown to the virtual
     * keyboard hidden.
     *
     * This tests the mode where the author opts in to the keyboard resizing page content.
     */
    @Test
    @MediumTest
    public void testVirtualKeyboardResizesContentNew() throws Throwable {
        startTest(VirtualKeyboardMode.RESIZES_CONTENT, CAPTURE_NEW);

        Bitmap expected;

        // Since this test checks new state, the keyboard will be down so all that must be done
        // for the reference screenshot is to update the DOM to the final state.
        {
            JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(), "updateDOM()");
            waitForFramePresented();

            expected = takeScreenshot();

            JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(), "undoUpdateDOM()");
        }

        showAndWaitForKeyboard();

        createTransitionAndWaitUntilDomUpdateDispatched();

        // Note: rendering is blocked here so we can't wait for the keyboard to hide since the size
        // depends on the renderer.
        hideKeyboard();

        // Start the animation. The "animation" simply displays the new transition for the full
        // duration of the test.
        startTransitionAnimation();

        waitForKeyboardHidden();

        // Wait for a frame to be presented to ensure the animation has started and the updated
        // viewport size is rendered.
        waitForFramePresented();

        Bitmap actual = takeScreenshot();

        Assert.assertTrue(doPixelTestScreenshotsMatch(expected, actual));
    }
}
