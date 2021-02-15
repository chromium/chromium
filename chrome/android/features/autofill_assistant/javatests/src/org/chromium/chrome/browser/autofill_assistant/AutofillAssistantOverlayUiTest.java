// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementExists;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getBoundingRectForElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getViewport;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitForElementRemoved;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayImage;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;

import java.util.Collections;
import java.util.concurrent.ExecutionException;

/**
 * Tests for the Autofill Assistant overlay.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantOverlayUiTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    // TODO(crbug.com/806868): Create a more specific test site for overlay testing.
    private static final String TEST_PAGE =
            "/components/test/data/autofill_assistant/html/autofill_assistant_target_website.html";

    @Before
    public void setUp() {
        mTestRule.startCustomTabActivityWithIntent(
                AutofillAssistantUiTestUtil.createMinimalCustomTabIntentForAutobot(
                        mTestRule.getTestServer().getURL(TEST_PAGE),
                        /* startImmediately = */ true));
        mTestRule.getActivity()
                .getRootUiCoordinatorForTesting()
                .getScrimCoordinator()
                .disableAnimationForTesting(true);
    }

    private WebContents getWebContents() {
        return mTestRule.getWebContents();
    }

    /** Creates a coordinator for use in UI tests with a default, non-null overlay image. */
    private AssistantOverlayCoordinator createCoordinator(AssistantOverlayModel model)
            throws ExecutionException {
        return createCoordinator(model,
                BitmapFactory.decodeResource(mTestRule.getActivity().getResources(),
                        org.chromium.chrome.autofill_assistant.R.drawable.btn_close));
    }

    /** Creates a coordinator for use in UI tests with a custom overlay image. */
    private AssistantOverlayCoordinator createCoordinator(
            AssistantOverlayModel model, @Nullable Bitmap overlayImage) throws ExecutionException {
        ChromeActivity activity = mTestRule.getActivity();
        return runOnUiThreadBlocking(()
                                             -> new AssistantOverlayCoordinator(activity,
                                                     activity.getBrowserControlsManager(),
                                                     activity.getCompositorViewHolder(),
                                                     mTestRule.getActivity()
                                                             .getRootUiCoordinatorForTesting()
                                                             .getScrimCoordinator(),
                                                     model));
    }

    /** Tests assumptions about the initial state of the infobox. */
    @Test
    @MediumTest
    public void testInitialState() throws Exception {
        AssistantOverlayModel model = new AssistantOverlayModel();
        AssistantOverlayCoordinator coordinator = createCoordinator(model);

        assertScrimDisplayed(false);
        tapElement("touch_area_one");
        waitForElementRemoved(getWebContents(), "touch_area_one");
    }

    /** Tests assumptions about the full overlay. */
    @Test
    @MediumTest
    public void testFullOverlay() throws Exception {
        AssistantOverlayModel model = new AssistantOverlayModel();
        AssistantOverlayCoordinator coordinator = createCoordinator(model);

        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.STATE, AssistantOverlayState.FULL));
        assertScrimDisplayed(true);
        tapElement("touch_area_one");
        assertThat(checkElementExists(getWebContents(), "touch_area_one"), is(true));

        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.STATE, AssistantOverlayState.HIDDEN));
        assertScrimDisplayed(false);
        tapElement("touch_area_one");
        waitForElementRemoved(getWebContents(), "touch_area_one");
    }

    /** Tests assumptions about the full overlay. */
    @Test
    @MediumTest
    public void testFullOverlayWithImage() throws Exception {
        AssistantOverlayModel model = new AssistantOverlayModel();
        AssistantOverlayCoordinator coordinator = createCoordinator(model);

        AssistantOverlayImage image = new AssistantOverlayImage(
                64, 64, 40, "example.com", Color.parseColor("#B3FFFFFF"), 40);
        runOnUiThreadBlocking(() -> {
            model.set(AssistantOverlayModel.STATE, AssistantOverlayState.FULL);
            model.set(AssistantOverlayModel.OVERLAY_IMAGE, image);
        });
        assertScrimDisplayed(true);
        // TODO(b/143452916): Test that the overlay image is actually being displayed.
    }

    /** Tests assumptions about the partial overlay. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1172616")
    public void testPartialOverlay() throws Exception {
        AssistantOverlayModel model = new AssistantOverlayModel();
        AssistantOverlayCoordinator coordinator = createCoordinator(model);

        // Partial overlay, no touchable areas: equivalent to full overlay.
        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.STATE, AssistantOverlayState.PARTIAL));
        assertScrimDisplayed(true);
        tapElement("touch_area_one");
        assertThat(checkElementExists(getWebContents(), "touch_area_one"), is(true));

        Rect rect = getBoundingRectForElement(getWebContents(), "touch_area_one");
        runOnUiThreadBlocking(()
                                      -> model.set(AssistantOverlayModel.TOUCHABLE_AREA,
                                              Collections.singletonList(new RectF(rect))));

        // Touchable area set, but no viewport given: equivalent to full overlay.
        tapElement("touch_area_one");
        assertThat(checkElementExists(getWebContents(), "touch_area_one"), is(true));

        // Set viewport.
        Rect viewport = getViewport(getWebContents());
        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.VISUAL_VIEWPORT, new RectF(viewport)));

        // Now the partial overlay allows tapping the highlighted touch area.
        tapElement("touch_area_one");
        waitForElementRemoved(getWebContents(), "touch_area_one");

        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.TOUCHABLE_AREA, Collections.emptyList()));
        tapElement("touch_area_four");
        assertThat(checkElementExists(getWebContents(), "touch_area_four"), is(true));
    }

    /** Scrolls a touchable area into view and then taps it. */
    @Test
    @MediumTest
    public void testSimpleScrollPartialOverlay() throws Exception {
        AssistantOverlayModel model = new AssistantOverlayModel();
        AssistantOverlayCoordinator coordinator = createCoordinator(model);

        scrollIntoViewIfNeeded("touch_area_five");
        Rect rect = getBoundingRectForElement(getWebContents(), "touch_area_five");
        Rect viewport = getViewport(getWebContents());
        runOnUiThreadBlocking(() -> {
            model.set(AssistantOverlayModel.STATE, AssistantOverlayState.PARTIAL);
            model.set(AssistantOverlayModel.VISUAL_VIEWPORT, new RectF(viewport));
            model.set(AssistantOverlayModel.TOUCHABLE_AREA,
                    Collections.singletonList(new RectF(rect)));
        });
        assertScrimDisplayed(true);
        tapElement("touch_area_five");
        waitForElementRemoved(getWebContents(), "touch_area_five");
    }

    /**
     * Regular overlay image test. Since there is no easy way to test whether the image is actually
     * rendered, this is simply checking that nothing crashes.
     */
    @Test
    @MediumTest
    public void testOverlayImageDoesNotCrashIfValid() throws Exception {
        AssistantOverlayModel model = new AssistantOverlayModel();
        Bitmap bitmap = BitmapFactory.decodeResource(mTestRule.getActivity().getResources(),
                org.chromium.chrome.autofill_assistant.R.drawable.btn_close);
        assertThat(bitmap, notNullValue());
        AssistantOverlayCoordinator coordinator =
                createCoordinator(model, /* overlayImage = */ bitmap);

        runOnUiThreadBlocking(() -> {
            model.set(AssistantOverlayModel.STATE, AssistantOverlayState.FULL);
            model.set(AssistantOverlayModel.OVERLAY_IMAGE,
                    new AssistantOverlayImage(32, 32, 12, "Text", Color.RED, 20));
        });

        assertScrimDisplayed(true);
    }

    /** Simulates what would happen if the overlay image fetcher returned null. */
    @Test
    @MediumTest
    public void testOverlayDoesNotCrashIfImageFailsToLoad() throws Exception {
        AssistantOverlayModel model = new AssistantOverlayModel();
        AssistantOverlayCoordinator coordinator =
                createCoordinator(model, /* overlayImage = */ null);

        runOnUiThreadBlocking(() -> {
            model.set(AssistantOverlayModel.STATE, AssistantOverlayState.FULL);
            model.set(AssistantOverlayModel.OVERLAY_IMAGE,
                    new AssistantOverlayImage(32, 32, 12, "Text", Color.RED, 20));
        });

        assertScrimDisplayed(true);
    }

    private void assertScrimDisplayed(boolean expected) throws Exception {
        // Wait for UI thread to be idle.
        onView(isRoot()).check(matches(isDisplayed()));

        View scrim = mTestRule.getActivity()
                             .getRootUiCoordinatorForTesting()
                             .getScrimCoordinator()
                             .getViewForTesting();

        // The scrim view is only attached to the view hierarchy when needed, preventing us from
        // using regular espresso facilities.
        boolean scrimInHierarchy =
                runOnUiThreadBlocking(() -> scrim != null && scrim.getParent() != null);

        if (expected) {
            assertTrue(
                    "The scrim wasn't in the hierarchy but was expected to be!", scrimInHierarchy);
            onView(is(scrim)).check(matches(isDisplayed()));
        } else {
            assertFalse(
                    "The scrim was in the hierarchy but wasn't expected to be!", scrimInHierarchy);
        }
    }

    void tapElement(String elementId) throws Exception {
        AutofillAssistantUiTestUtil.tapElement(mTestRule, elementId);
    }

    /**
     * Scrolls to the specified element on the webpage, if necessary.
     */
    private void scrollIntoViewIfNeeded(String elementId) throws Exception {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(getWebContents(),
                "(function() {"
                        + " document.getElementById('" + elementId + "').scrollIntoViewIfNeeded();"
                        + " return true;"
                        + "})()");
        javascriptHelper.waitUntilHasValue();
    }
}
