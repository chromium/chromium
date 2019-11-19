// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.PositionAssertions.isRightOf;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.verify;

import android.support.design.widget.CoordinatorLayout;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip.Icon;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderCoordinator;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.ui.widget.MaterialProgressBar;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the Autofill Assistant header.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillAssistantHeaderUiTest {
    private static class ViewHolder {
        private final TextView mStatusMessage;
        private final MaterialProgressBar mProgressBar;
        private final View mProfileIcon;

        private ViewHolder(View rootView) {
            mStatusMessage = rootView.findViewById(R.id.status_message);
            mProgressBar = rootView.findViewById(R.id.progress_bar);
            mProfileIcon = rootView.findViewById(R.id.profile_image);
        }
    }

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    public Runnable mRunnableMock;

    @Before
    public void setUp() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), "about:blank"));
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    /** Creates a coordinator for use in UI tests, and adds it to the global view hierarchy. */
    private AssistantHeaderCoordinator createCoordinator(AssistantHeaderModel model) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            AssistantHeaderCoordinator coordinator =
                    new AssistantHeaderCoordinator(getActivity(), model);

            CoordinatorLayout.LayoutParams lp = new CoordinatorLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            lp.gravity = Gravity.BOTTOM;

            ViewGroup chromeCoordinatorView = getActivity().findViewById(R.id.coordinator);
            chromeCoordinatorView.addView(coordinator.getView(), lp);
            coordinator.disableAnimationsForTesting(true);

            return coordinator;
        });
    }

    @Test
    @MediumTest
    public void testInitialState() {
        AssistantHeaderModel model = new AssistantHeaderModel();
        AssistantHeaderCoordinator coordinator = createCoordinator(model);
        ViewHolder viewHolder = new ViewHolder(coordinator.getView());

        onView(is(viewHolder.mStatusMessage))
                .check(matches(isDisplayed()))
                .check(matches(withText("")));

        onView(is(viewHolder.mProgressBar))
                .check(matches(isDisplayed()))
                .check(matches(hasProgress(0)));

        onView(is(viewHolder.mProfileIcon)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSimpleModelChanges() {
        AssistantHeaderModel model = new AssistantHeaderModel();
        AssistantHeaderCoordinator coordinator = createCoordinator(model);
        ViewHolder viewHolder = new ViewHolder(coordinator.getView());

        String statusMessage = "Hello World";
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantHeaderModel.STATUS_MESSAGE, statusMessage));

        onView(is(viewHolder.mStatusMessage))
                .check(matches(isDisplayed()))
                .check(matches(withText(statusMessage)));

        int progress = 42;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantHeaderModel.PROGRESS, progress));

        onView(is(viewHolder.mProgressBar))
                .check(matches(isDisplayed()))
                .check(matches(hasProgress(progress)));
    }

    @Test
    @MediumTest
    public void testChip() {
        AssistantHeaderModel model = new AssistantHeaderModel();
        AssistantHeaderCoordinator coordinator = createCoordinator(model);

        String chipText = "Hello World";
        AssistantChip chip = new AssistantChip(AssistantChip.Type.BUTTON_FILLED_BLUE, Icon.DONE,
                chipText, /* disabled= */ false, /* sticky= */ false, () -> {});

        // Set the header chip without displaying it.
        TestThreadUtils.runOnUiThreadBlocking(() -> model.set(AssistantHeaderModel.CHIP, chip));

        Matcher<View> chipMatcher =
                allOf(isDescendantOfA(is(coordinator.getView())), withText(chipText));
        onView(chipMatcher).check(matches(not(isDisplayed())));

        // Show the chip
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantHeaderModel.CHIP_VISIBLE, true));
        onView(chipMatcher)
                .check(matches(isDisplayed()))
                .check(isRightOf(withId(R.id.status_message)));
    }

    @Test
    @MediumTest
    public void testProfileImageMenu() {
        AssistantHeaderModel model = new AssistantHeaderModel();
        AssistantHeaderCoordinator coordinator = createCoordinator(model);
        ViewHolder viewHolder = new ViewHolder(coordinator.getView());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantHeaderModel.FEEDBACK_BUTTON_CALLBACK, mRunnableMock));

        onView(is(viewHolder.mProfileIcon)).perform(click());

        onView(withText(R.string.autofill_assistant_send_feedback)).perform(click());

        verify(mRunnableMock).run();

        // TODO(crbug.com/806868): Test click on the "Settings" menu item.
    }

    private static Matcher<View> hasProgress(int expectedProgress) {
        return new BaseMatcher<View>() {
            @Override
            public boolean matches(Object o) {
                return ((MaterialProgressBar) o).getProgressForTesting() == expectedProgress;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("hasProgress: " + expectedProgress);
            }
        };
    }
}
