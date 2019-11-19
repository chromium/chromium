// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBox;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBoxCoordinator;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBoxModel;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the Autofill Assistant infobox.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantInfoBoxUiTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private TextView getExplanationView(AssistantInfoBoxCoordinator coordinator) {
        return coordinator.getView().findViewById(R.id.info_box_explanation);
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
        AutofillAssistantUiTestUtil.startOnBlankPage(mTestRule);
    }

    /** Tests assumptions about the initial state of the infobox. */
    @Test
    @MediumTest
    public void testInitialState() throws Exception {
        AssistantInfoBoxModel model = new AssistantInfoBoxModel();
        AssistantInfoBoxCoordinator coordinator = createCoordinator(model);

        assertThat(model.get(AssistantInfoBoxModel.INFO_BOX), nullValue());
        onView(is(coordinator.getView())).check(matches(not(isDisplayed())));
    }

    /** Tests for an infobox with a message, but without an image. */
    @Test
    @MediumTest
    public void testMessageNoImage() throws Exception {
        AssistantInfoBoxModel model = new AssistantInfoBoxModel();
        AssistantInfoBoxCoordinator coordinator = createCoordinator(model);
        AssistantInfoBox infoBox = new AssistantInfoBox("", "Message");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantInfoBoxModel.INFO_BOX, infoBox));
        onView(is(coordinator.getView())).check(matches(isDisplayed()));
        // Image should not be set.
        assertThat(getExplanationView(coordinator).getCompoundDrawables()[1], nullValue());
        onView(is(getExplanationView(coordinator))).check(matches(withText("Message")));
    }

    /** Tests for an infobox with message and image. */
    @Test
    @MediumTest
    public void testImage() throws Exception {
        AssistantInfoBoxModel model = new AssistantInfoBoxModel();
        AssistantInfoBoxCoordinator coordinator = createCoordinator(model);
        AssistantInfoBox infoBox = new AssistantInfoBox("x", "Message");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantInfoBoxModel.INFO_BOX, infoBox));
        onView(is(getExplanationView(coordinator))).check(matches(isDisplayed()));
        // Image should be set.
        assertThat(getExplanationView(coordinator).getCompoundDrawables()[1], not(nullValue()));
        onView(is(getExplanationView(coordinator))).check(matches(withText("Message")));
    }

    @Test
    @MediumTest
    public void testVisibility() throws Exception {
        AssistantInfoBoxModel model = new AssistantInfoBoxModel();
        AssistantInfoBoxCoordinator coordinator = createCoordinator(model);
        AssistantInfoBox infoBox = new AssistantInfoBox("", "");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantInfoBoxModel.INFO_BOX, infoBox));
        onView(is(coordinator.getView())).check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantInfoBoxModel.INFO_BOX, null));
        onView(is(coordinator.getView())).check(matches(not(isDisplayed())));
    }
}
