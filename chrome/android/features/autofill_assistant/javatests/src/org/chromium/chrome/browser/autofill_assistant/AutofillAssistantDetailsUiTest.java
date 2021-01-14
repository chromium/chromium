// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_AUTO;
import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.hasTypefaceSpan;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.hasTypefaceStyle;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.isImportantForAccessibility;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.isTextMaxLines;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Typeface;
import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetails;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsCoordinator;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsModel;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantPlaceholdersConfiguration;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.Arrays;

/** Tests for the Autofill Assistant details. */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantDetailsUiTest {
    public static final AssistantPlaceholdersConfiguration NO_PLACEHOLDERS =
            new AssistantPlaceholdersConfiguration(
                    /* showImagePlaceholder= */ false,
                    /* showTitlePlaceholder= */ false,
                    /* showDescriptionLine1Placeholder= */ false,
                    /* showDescriptionLine2Placeholder= */ false,
                    /* showDescriptionLine3Placeholder= */ false);

    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    /**
     * Matchers for the different views of a single details item in the details list. We need to use
     * matchers (instead of directly querying with findViewById) because the inflation of the
     * details views if postponed to the UI-thread when adding items to the list, so we can't assume
     * that the details views are available directly after adding items to the details list.
     */
    private static class ViewMatchers {
        final Matcher<View> mImageView;
        final Matcher<View> mTitleView;
        final Matcher<View> mDescriptionLine1View;
        final Matcher<View> mDescriptionLine2View;
        final Matcher<View> mDescriptionLine3View;
        final Matcher<View> mPriceAttributionView;
        final Matcher<View> mPriceView;
        final Matcher<View> mTotalPriceLabelView;
        final Matcher<View> mTotalPriceView;

        ViewMatchers(View detailsListView) {
            mImageView = descendantWithId(detailsListView, R.id.details_image);
            mTitleView = descendantWithId(detailsListView, R.id.details_title);
            mDescriptionLine1View = descendantWithId(detailsListView, R.id.details_line1);
            mDescriptionLine2View = descendantWithId(detailsListView, R.id.details_line2);
            mDescriptionLine3View = descendantWithId(detailsListView, R.id.details_line3);
            mPriceAttributionView =
                    descendantWithId(detailsListView, R.id.details_price_attribution);
            mPriceView = descendantWithId(detailsListView, R.id.details_price);
            mTotalPriceView = descendantWithId(detailsListView, R.id.details_total_price);
            mTotalPriceLabelView =
                    descendantWithId(detailsListView, R.id.details_total_price_label);
        }

        private Matcher<View> descendantWithId(View ancestor, int id) {
            return allOf(isDescendantOfA(is(ancestor)), withId(id));
        }
    }

    /** Creates a coordinator for use in UI tests, and adds it to the global view hierarchy. */
    private AssistantDetailsCoordinator createCoordinator(AssistantDetailsModel model)
            throws Exception {
        AssistantDetailsCoordinator coordinator = runOnUiThreadBlocking(() -> {
            Bitmap testImage = BitmapFactory.decodeResource(
                    mTestRule.getActivity().getResources(), R.drawable.btn_close);

            return new AssistantDetailsCoordinator(InstrumentationRegistry.getTargetContext(),
                    model, new AutofillAssistantUiTestUtil.MockImageFetcher(testImage, null));
        });

        runOnUiThreadBlocking(()
                                      -> AutofillAssistantUiTestUtil.attachToCoordinator(
                                              mTestRule.getActivity(), coordinator.getView()));
        return coordinator;
    }

    private static void setDetails(AssistantDetailsModel model, AssistantDetails... details) {
        runOnUiThreadBlocking(() -> model.setDetailsList(Arrays.asList(details)));

        // Wait for the main thread to be idle (i.e. the UI should be stable).
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @Before
    public void setUp() {
        AutofillAssistantUiTestUtil.startOnBlankPage(mTestRule);
    }

    /** Tests assumptions about the initial state of the details. */
    @Test
    @MediumTest
    public void testInitialState() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);

        assertThat(model.get(AssistantDetailsModel.DETAILS).size(), is(0));
        assertThat(coordinator.getView().getChildCount(), is(0));
    }

    /** Tests visibility of views. */
    @Test
    @MediumTest
    public void testVisibility() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewMatchers viewMatchers = new ViewMatchers(coordinator.getView());

        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "line 1",
                        "line 2", "", "line 3", false, false, false, false, false,
                        NO_PLACEHOLDERS));

        onView(viewMatchers.mImageView).check(matches(isDisplayed()));
        onView(viewMatchers.mTitleView).check(matches(isDisplayed()));
        onView(viewMatchers.mDescriptionLine1View).check(matches(isDisplayed()));
        onView(viewMatchers.mDescriptionLine2View).check(matches(isDisplayed()));
        onView(viewMatchers.mDescriptionLine3View).check(matches(not(isDisplayed())));
        onView(viewMatchers.mPriceView).check(matches(isDisplayed()));
        onView(viewMatchers.mTotalPriceLabelView).check(matches(isDisplayed()));
        onView(viewMatchers.mTotalPriceView).check(matches(isDisplayed()));
        onView(viewMatchers.mPriceAttributionView).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAccessibility() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewMatchers viewMatchers = new ViewMatchers(coordinator.getView());

        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "line 1",
                        "line 2", "", "line 3", false, false, false, false, false,
                        NO_PLACEHOLDERS));

        onView(viewMatchers.mImageView).check(matches(withContentDescription("hint")));
        onView(viewMatchers.mImageView)
                .check(matches(isImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_AUTO)));
    }

    @Test
    @MediumTest
    public void testAccessibilityEmpty() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewMatchers viewMatchers = new ViewMatchers(coordinator.getView());

        setDetails(model,
                new AssistantDetails("title", "image", "", null, "Total", "$12", "line 1", "line 2",
                        "", "line 3", false, false, false, false, false, NO_PLACEHOLDERS));

        onView(viewMatchers.mImageView).check(matches(withContentDescription("")));
        onView(viewMatchers.mImageView)
                .check(matches(isImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO)));
    }

    @Test
    @MediumTest
    public void testTitle() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewMatchers viewMatchers = new ViewMatchers(coordinator.getView());

        /* Description lines 1 and 2 are set, title must be in single line. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "line 1",
                        "line 2", "", "price", false, false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mTitleView).check(matches(isTextMaxLines(1)));
        onView(viewMatchers.mTitleView).check(matches(allOf(withText("title"), isDisplayed())));

        /* Description line 1 is set, title must be max 2 lines. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "line 1", "",
                        "", "price", false, false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mTitleView).check(matches(isTextMaxLines(2)));
        onView(viewMatchers.mTitleView).check(matches(allOf(withText("title"), isDisplayed())));

        /* Description line 2 is set, title must be max 2 lines. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "", "line 2",
                        "", "price", false, false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mTitleView).check(matches(isTextMaxLines(2)));
        onView(viewMatchers.mTitleView).check(matches(allOf(withText("title"), isDisplayed())));

        /* None of description line 1 or 2 is set, title must be max 3 lines. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "", "", "",
                        "price", false, false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mTitleView).check(matches(isTextMaxLines(3)));
        onView(viewMatchers.mTitleView).check(matches(allOf(withText("title"), isDisplayed())));

        /* There is a placeholder for description line 1, title must be max 2 lines. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "", "", "",
                        "price", false, false, false, false, false,
                        new AssistantPlaceholdersConfiguration(
                                /* showImagePlaceholder= */ false,
                                /* showTitlePlaceholder= */ false,
                                /* showDescriptionLine1Placeholder= */ true,
                                /* showDescriptionLine2Placeholder= */ false,
                                /* showDescriptionLine3Placeholder= */ false)));
        onView(viewMatchers.mTitleView).check(matches(isTextMaxLines(2)));

        /* There is a placeholder for description line 1 & 2, title must be max 1 line. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "", "", "",
                        "price", false, false, false, false, false,
                        new AssistantPlaceholdersConfiguration(
                                /* showImagePlaceholder= */ false,
                                /* showTitlePlaceholder= */ false,
                                /* showDescriptionLine1Placeholder= */ true,
                                /* showDescriptionLine2Placeholder= */ true,
                                /* showDescriptionLine3Placeholder= */ false)));
        onView(viewMatchers.mTitleView).check(matches(isTextMaxLines(1)));
    }

    @Test
    @MediumTest
    public void testDescriptionLine1() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewMatchers viewMatchers = new ViewMatchers(coordinator.getView());

        /* Description line 1 is set and should be visible. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "", "", "line 1", "", "", "",
                        false, false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mDescriptionLine1View)
                .check(matches(allOf(withText("line 1"), isDisplayed())));

        /* Description line 1 is not set and should be invisible. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "", "", "", "", "", "", false,
                        false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mDescriptionLine1View).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testDescriptionLine2() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewMatchers viewMatchers = new ViewMatchers(coordinator.getView());

        /* Description line 2 is set and should be visible. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "", "", "", "line 2", "", "",
                        false, false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mDescriptionLine2View)
                .check(matches(allOf(withText("line 2"), isDisplayed())));

        /* Description line 2 is not set and should be invisible. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "", "", "", "", "", "", false,
                        false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mDescriptionLine2View).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testDescriptionLine3() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewMatchers viewMatchers = new ViewMatchers(coordinator.getView());

        /* Description line 3 is set and should be visible. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "", "", "", "", "line 3", "",
                        false, false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mDescriptionLine3View)
                .check(matches(allOf(withText("line 3"), isDisplayed())));

        /* Description line 3 is not set and should be invisible. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "", "", "", "", "", "line 3",
                        false, false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mDescriptionLine3View).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testPriceAttribution() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewMatchers viewMatchers = new ViewMatchers(coordinator.getView());

        /* Price attribution is set and should be visible. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "", "", "",
                        "price attribution", false, false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mPriceAttributionView)
                .check(matches(allOf(withText("price attribution"), isDisplayed())));

        /* Price attribution is not set and should be invisible. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "", "", "", "", "", "", false,
                        false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mPriceAttributionView).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testHighlighting() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewMatchers viewMatchers = new ViewMatchers(coordinator.getView());

        /* Check that title is highlighted. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "line 1",
                        "line 2", "line 3", "Est. total", true, true, false, false, false,
                        NO_PLACEHOLDERS));
        onView(viewMatchers.mTitleView).check(matches(hasTypefaceStyle(Typeface.BOLD_ITALIC)));
        onView(viewMatchers.mDescriptionLine1View)
                .check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));
        onView(viewMatchers.mDescriptionLine2View)
                .check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));
        onView(viewMatchers.mDescriptionLine3View)
                .check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));
        onView(viewMatchers.mPriceAttributionView)
                .check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));

        /* Check that description 1 is highlighted. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "line 1",
                        "line 2", "line 3", "Est. total", true, false, true, false, false,
                        NO_PLACEHOLDERS));
        onView(viewMatchers.mTitleView).check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));
        onView(viewMatchers.mDescriptionLine1View)
                .check(matches(hasTypefaceStyle(Typeface.BOLD_ITALIC)));
        onView(viewMatchers.mDescriptionLine2View)
                .check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));
        onView(viewMatchers.mDescriptionLine3View)
                .check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));
        onView(viewMatchers.mPriceAttributionView)
                .check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));

        /* Check that description 2 is highlighted. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "line 1",
                        "line 2", "line 3", "Est. total", true, false, false, true, false,
                        NO_PLACEHOLDERS));
        onView(viewMatchers.mTitleView).check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));
        onView(viewMatchers.mDescriptionLine1View)
                .check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));
        onView(viewMatchers.mDescriptionLine2View)
                .check(matches(hasTypefaceStyle(Typeface.BOLD_ITALIC)));
        onView(viewMatchers.mDescriptionLine3View)
                .check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));
        onView(viewMatchers.mPriceAttributionView)
                .check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));

        /* Check that description 3 and price attribution are highlighted. */
        setDetails(model,
                new AssistantDetails("title", "image", "hint", null, "Total", "$12", "line 1",
                        "line 2", "line 3", "Est. total", true, false, false, false, true,
                        NO_PLACEHOLDERS));
        onView(viewMatchers.mTitleView).check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));
        onView(viewMatchers.mDescriptionLine1View)
                .check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));
        onView(viewMatchers.mDescriptionLine2View)
                .check(matches(not(hasTypefaceStyle(Typeface.BOLD_ITALIC))));
        onView(viewMatchers.mDescriptionLine3View)
                .check(matches(hasTypefaceStyle(Typeface.BOLD_ITALIC)));
        onView(viewMatchers.mPriceAttributionView)
                .check(matches(hasTypefaceStyle(Typeface.BOLD_ITALIC)));
    }

    @Test
    @MediumTest
    public void testStyleSpans() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        createCoordinator(model);

        setDetails(model,
                new AssistantDetails("<b>title</b>", "image", "hint", null, "<b>Total</b>",
                        "<b>$12</b>", "<b>line 1</b>", "<b>line 2</b>", "", "<b>line 3</b>", false,
                        false, false, false, false, NO_PLACEHOLDERS));

        onView(withText("title"))
                .check(matches(hasTypefaceSpan(0, "title".length() - 1, Typeface.BOLD)));
        onView(withText("Total"))
                .check(matches(hasTypefaceSpan(0, "Total".length() - 1, Typeface.BOLD)));
        onView(withText("$12"))
                .check(matches(hasTypefaceSpan(0, "$12".length() - 1, Typeface.BOLD)));
        onView(withText("line 1"))
                .check(matches(hasTypefaceSpan(0, "line 1".length() - 1, Typeface.BOLD)));
        onView(withText("line 2"))
                .check(matches(hasTypefaceSpan(0, "line 2".length() - 1, Typeface.BOLD)));
        onView(withText("line 3"))
                .check(matches(hasTypefaceSpan(0, "line 3".length() - 1, Typeface.BOLD)));
    }

    @Test
    @MediumTest
    public void testPlaceholders() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewMatchers viewMatchers = new ViewMatchers(coordinator.getView());

        // Without placeholders, the image and descriptions are hidden. The title is always
        // displayed.
        setDetails(model,
                new AssistantDetails("title", "", "hint", null, "", "", "", "", "", "", false,
                        false, false, false, false, NO_PLACEHOLDERS));
        onView(viewMatchers.mTitleView).check(matches(isDisplayed()));
        onView(viewMatchers.mImageView).check(matches(not(isDisplayed())));
        onView(viewMatchers.mDescriptionLine1View).check(matches(not(isDisplayed())));
        onView(viewMatchers.mDescriptionLine2View).check(matches(not(isDisplayed())));
        onView(viewMatchers.mDescriptionLine3View).check(matches(not(isDisplayed())));

        // With placeholders, the image and descriptions are displayed.
        setDetails(model,
                new AssistantDetails("title", "", "hint", null, "", "", "", "", "", "", false,
                        false, false, false, false,
                        new AssistantPlaceholdersConfiguration(
                                /* showImagePlaceholder= */ true,
                                /* showTitlePlaceholder= */ true,
                                /* showDescriptionLine1Placeholder= */ true,
                                /* showDescriptionLine2Placeholder= */ true,
                                /* showDescriptionLine3Placeholder= */ true)));
        onView(viewMatchers.mTitleView).check(matches(isDisplayed()));
        onView(viewMatchers.mImageView).check(matches(isDisplayed()));
        onView(viewMatchers.mDescriptionLine1View).check(matches(isDisplayed()));
        onView(viewMatchers.mDescriptionLine2View).check(matches(isDisplayed()));
        onView(viewMatchers.mDescriptionLine3View).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testMultipleDetails() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewMatchers viewMatchers = new ViewMatchers(coordinator.getView());

        setDetails(model,
                new AssistantDetails("title 1", "", "", null, "", "", "", "", "", "", false, false,
                        false, false, false, NO_PLACEHOLDERS),
                new AssistantDetails("title 2", "", "", null, "", "", "", "", "", "", false, false,
                        false, false, false, NO_PLACEHOLDERS),
                new AssistantDetails("title 3", "", "", null, "", "", "", "", "", "", false, false,
                        false, false, false, NO_PLACEHOLDERS));

        onView(allOf(viewMatchers.mTitleView, withText("title 1"))).check(matches(isDisplayed()));
        onView(allOf(viewMatchers.mTitleView, withText("title 2"))).check(matches(isDisplayed()));
        onView(allOf(viewMatchers.mTitleView, withText("title 3"))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testPlaceholdersAnimation() throws Exception {
        // Test that the placeholders animation is running only when details have placeholders.
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);

        assertThat(coordinator.isRunningPlaceholdersAnimationForTesting(), is(false));
        setDetails(model,
                new AssistantDetails("title 1", "", "", null, "", "", "", "", "", "", false, false,
                        false, false, false, NO_PLACEHOLDERS));
        assertThat(coordinator.isRunningPlaceholdersAnimationForTesting(), is(false));
        setDetails(model,
                new AssistantDetails("title 1", "", "", null, "", "", "", "", "", "", false, false,
                        false, false, false,
                        new AssistantPlaceholdersConfiguration(
                                /* showImagePlaceholder= */ true,
                                /* showTitlePlaceholder= */ false,
                                /* showDescriptionLine1Placeholder= */ false,
                                /* showDescriptionLine2Placeholder= */ false,
                                /* showDescriptionLine3Placeholder= */ false)));
        assertThat(coordinator.isRunningPlaceholdersAnimationForTesting(), is(true));
        setDetails(model);
        assertThat(coordinator.isRunningPlaceholdersAnimationForTesting(), is(false));
    }
}
