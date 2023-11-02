// Copyright 2019 The Chromium Authors
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
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementOnScreen;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getBoundingRectForElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.scrollIntoViewIfNeeded;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitForElementRemoved;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.RectF;
import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.json.JSONArray;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayImage;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayModel.AssistantOverlayRect;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

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
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        mTestRule.getTestServer().getURL(TEST_PAGE)));
        mTestRule.getActivity()
                .getRootUiCoordinatorForTesting()
                .getScrimCoordinator()
                .disableAnimationForTesting(true);
    }

    private WebContents getWebContents() {
        return mTestRule.getWebContents();
    }

    private AssistantOverlayModel createModel() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(AssistantOverlayModel::new);
    }

    /** Creates a coordinator for use in UI tests with a default, non-null overlay image. */
    private AssistantOverlayCoordinator createCoordinator(AssistantOverlayModel model)
            throws ExecutionException {
        return createCoordinator(model,
                BitmapFactory.decodeResource(mTestRule.getActivity().getResources(),
                        org.chromium.components.autofill_assistant.R.drawable.btn_close));
    }

    /** Creates a coordinator for use in UI tests with a custom overlay image. */
    private AssistantOverlayCoordinator createCoordinator(
            AssistantOverlayModel model, @Nullable Bitmap overlayImage) throws ExecutionException {
        ChromeActivity activity = mTestRule.getActivity();
        return runOnUiThreadBlocking(() -> {
            return new AssistantOverlayCoordinator(activity,
                    ()
                            -> new AssistantBrowserControlsChrome(
                                    activity.getBrowserControlsManager()),
                    activity.getCompositorViewHolderForTesting(),
                    mTestRule.getActivity().getRootUiCoordinatorForTesting().getScrimCoordinator(),
                    model, new AssistantStaticDependenciesChrome().getAccessibilityUtil());
        });
    }

    /** Tests assumptions about the initial state of the infobox. */
    @Test
    @MediumTest
    public void testInitialState() throws Exception {
        AssistantOverlayModel model = createModel();
        AssistantOverlayCoordinator coordinator = createCoordinator(model);

        assertScrimDisplayed(false);
        tapElement("touch_area_one");
        waitForElementRemoved(getWebContents(), "touch_area_one");
    }

    /** Tests assumptions about the full overlay. */
    @Test
    @MediumTest
    public void testFullOverlay() throws Exception {
        AssistantOverlayModel model = createModel();
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
        AssistantOverlayModel model = createModel();
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
    public void testPartialOverlay() throws Exception {
        AssistantOverlayModel model = createModel();
        AssistantOverlayCoordinator coordinator = createCoordinator(model);

        // Partial overlay, no touchable areas: equivalent to full overlay.
        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.STATE, AssistantOverlayState.PARTIAL));
        assertScrimDisplayed(true);
        tapElement("touch_area_one");
        assertThat(checkElementExists(getWebContents(), "touch_area_one"), is(true));

        Rect rect = getBoundingRectForElement(getWebContents(), "touch_area_one");
        runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantOverlayModel.TOUCHABLE_AREA,
                                Collections.singletonList(new AssistantOverlayRect(rect))));

        // Touchable area set, but no viewport given: equivalent to full overlay.
        tapElement("touch_area_one");
        assertThat(checkElementExists(getWebContents(), "touch_area_one"), is(true));

        // Set WebContents.
        runOnUiThreadBlocking(
                () -> model.set(AssistantOverlayModel.WEB_CONTENTS, getWebContents()));

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
        AssistantOverlayModel model = createModel();
        createCoordinator(model);

        ChromeTabUtils.waitForInteractable(mTestRule.getActivity().getActivityTab());
        scrollIntoViewIfNeeded(mTestRule.getWebContents(), "touch_area_five");
        waitUntil(() -> checkElementOnScreen(mTestRule, "touch_area_five"));
        Rect rect = getBoundingRectForElement(getWebContents(), "touch_area_five");
        Rect viewport = getViewport(getWebContents());
        runOnUiThreadBlocking(() -> {
            model.set(AssistantOverlayModel.STATE, AssistantOverlayState.PARTIAL);
            model.set(AssistantOverlayModel.WEB_CONTENTS, getWebContents());
            model.set(AssistantOverlayModel.VISUAL_VIEWPORT, new RectF(viewport));
            model.set(AssistantOverlayModel.TOUCHABLE_AREA,
                    Collections.singletonList(new AssistantOverlayRect(rect)));
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
        AssistantOverlayModel model = createModel();
        Bitmap bitmap = BitmapFactory.decodeResource(mTestRule.getActivity().getResources(),
                org.chromium.components.autofill_assistant.R.drawable.btn_close);
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
        AssistantOverlayModel model = createModel();
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

    private void tapElement(String elementId) throws Exception {
        AutofillAssistantUiTestUtil.tapElement(mTestRule, elementId);
    }

    private Rect getViewport(WebContents webContents) throws Exception {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(webContents,
                "(function() {"
                        + " const v = window.visualViewport;"
                        + " return ["
                        + "   v.pageLeft, v.pageTop,"
                        + "   v.pageLeft + v.width, v.pageTop + v.height"
                        + " ];"
                        + "})()");
        javascriptHelper.waitUntilHasValue();
        JSONArray values = new JSONArray(javascriptHelper.getJsonResultAndClear());
        return new Rect(values.getInt(0), values.getInt(1), values.getInt(2), values.getInt(3));
    }
}
