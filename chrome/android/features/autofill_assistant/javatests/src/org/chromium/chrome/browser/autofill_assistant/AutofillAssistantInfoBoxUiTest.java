// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.hasTypefaceSpan;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Typeface;
import android.support.test.InstrumentationRegistry;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.generic_ui.AssistantDrawable;
import org.chromium.components.autofill_assistant.infobox.AssistantInfoBox;
import org.chromium.components.autofill_assistant.infobox.AssistantInfoBoxCoordinator;
import org.chromium.components.autofill_assistant.infobox.AssistantInfoBoxModel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Tests for the Autofill Assistant infobox. */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantInfoBoxUiTest {
    private static final AssistantDrawable sAssistantDrawable = AssistantDrawable.createFromIcon(1);
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private TextView getExplanationView(AssistantInfoBoxCoordinator coordinator) {
        return coordinator.getView().findViewById(R.id.info_box_explanation);
    }

    private ImageView getImageView(AssistantInfoBoxCoordinator coordinator) {
        return coordinator.getView().findViewById(R.id.info_box_image);
    }

    private AssistantInfoBoxModel createModel() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(AssistantInfoBoxModel::new);
    }

    /** Creates a coordinator for use in UI tests, and adds it to the global view hierarchy. */
    private AssistantInfoBoxCoordinator createCoordinator(AssistantInfoBoxModel model)
            throws Exception {
        AssistantInfoBoxCoordinator coordinator = TestThreadUtils.runOnUiThreadBlocking(() -> {
            Bitmap testImage = BitmapFactory.decodeResource(
                    mTestRule.getActivity().getResources(), R.drawable.btn_close);

            return new AssistantInfoBoxCoordinator(InstrumentationRegistry.getTargetContext(),
                    model, new AutofillAssistantUiTestUtil.MockImageFetcher(testImage, null));
        });

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillAssistantUiTestUtil.attachToCoordinator(
                                mTestRule.getActivity(), coordinator.getView()));
        return coordinator;
    }

    @Before
    public void setUp() {
        mTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), "about:blank"));
    }

    /** Tests assumptions about the initial state of the infobox. */
    @Test
    @MediumTest
    public void testInitialState() throws Exception {
        AssistantInfoBoxModel model = createModel();
        AssistantInfoBoxCoordinator coordinator = createCoordinator(model);

        assertThat(model.get(AssistantInfoBoxModel.INFO_BOX), nullValue());
        onView(is(coordinator.getView())).check(matches(not(isDisplayed())));
    }

    /** Tests for an infobox with a message, but without an image. */
    @Test
    @MediumTest
    public void testMessageNoImageLegacy() throws Exception {
        testMessageNoImage(/*useLegacyImplementation=*/false);
    }

    /** Tests for an infobox with a message, but without an image. */
    @Test
    @MediumTest
    public void testMessageNoImage() throws Exception {
        testMessageNoImage(/*useLegacyImplementation=*/true);
    }

    private void testMessageNoImage(boolean useLegacyImplementation) throws Exception {
        AssistantInfoBoxModel model = createModel();
        AssistantInfoBoxCoordinator coordinator = createCoordinator(model);
        AssistantInfoBox infoBox = new AssistantInfoBox(
                null, "Message", /* useIntrinsicDimensions= */ useLegacyImplementation);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantInfoBoxModel.INFO_BOX, infoBox));
        onView(is(coordinator.getView())).check(matches(isDisplayed()));
        // Image should not be set.org.chromium.components.autofill_assistant.generic_ui
        assertThat(getExplanationView(coordinator).getCompoundDrawables()[1], nullValue());
        onView(is(getImageView(coordinator))).check(matches(not(isDisplayed())));

        onView(is(getExplanationView(coordinator))).check(matches(withText("Message")));

        // Test that info message supports typeface span.
        AssistantInfoBox boldInfoBox = new AssistantInfoBox(
                null, "<b>Message</b>", /* useIntrinsicDimensions= */ useLegacyImplementation);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantInfoBoxModel.INFO_BOX, boldInfoBox));
        onView(is(getExplanationView(coordinator))).check(matches(withText("Message")));
        onView(withText("Message"))
                .check(matches(hasTypefaceSpan(0, "Message".length() - 1, Typeface.BOLD)));
    }

    /** Tests for an infobox with message and image. */
    @Test
    @MediumTest
    public void testImageLegacy() throws Exception {
        AssistantInfoBoxModel model = createModel();
        AssistantInfoBoxCoordinator coordinator = createCoordinator(model);
        AssistantInfoBox infoBox = new AssistantInfoBox(
                sAssistantDrawable, "Message", /* useIntrinsicDimensions= */ true);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantInfoBoxModel.INFO_BOX, infoBox));
        onView(is(getExplanationView(coordinator))).check(matches(isDisplayed()));
        // Image should be set.
        assertThat(getExplanationView(coordinator).getCompoundDrawables()[1], not(nullValue()));
        onView(is(getExplanationView(coordinator))).check(matches(withText("Message")));
    }

    /** Tests for an infobox with message and image. */
    @Test
    @MediumTest
    public void testImage() throws Exception {
        AssistantInfoBoxModel model = createModel();
        AssistantInfoBoxCoordinator coordinator = createCoordinator(model);
        AssistantInfoBox infoBox = new AssistantInfoBox(
                sAssistantDrawable, "Message", /* useIntrinsicDimensions= */ false);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantInfoBoxModel.INFO_BOX, infoBox));
        onView(is(getExplanationView(coordinator))).check(matches(isDisplayed()));
        onView(is(getImageView(coordinator))).check(matches(isDisplayed()));
        // Image should be set.
        assertThat(getImageView(coordinator).getDrawable(), not(nullValue()));
        onView(is(getExplanationView(coordinator))).check(matches(withText("Message")));
    }

    @Test
    @MediumTest
    public void hideIfEmpty() throws Exception {
        AssistantInfoBoxModel model = createModel();
        AssistantInfoBoxCoordinator coordinator = createCoordinator(model);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantInfoBoxModel.INFO_BOX,
                                new AssistantInfoBox(
                                        null, "Message", /* useIntrinsicDimensions= */ false)));
        onView(is(coordinator.getView())).check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantInfoBoxModel.INFO_BOX,
                                new AssistantInfoBox(
                                        null, "", /* useIntrinsicDimensions= */ false)));
        onView(is(coordinator.getView())).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void hideIfNull() throws Exception {
        AssistantInfoBoxModel model = createModel();
        AssistantInfoBoxCoordinator coordinator = createCoordinator(model);
        AssistantInfoBox infoBox =
                new AssistantInfoBox(null, "Message", /* useIntrinsicDimensions= */ false);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantInfoBoxModel.INFO_BOX, infoBox));
        onView(is(coordinator.getView())).check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantInfoBoxModel.INFO_BOX, null));
        onView(is(coordinator.getView())).check(matches(not(isDisplayed())));
    }
}
