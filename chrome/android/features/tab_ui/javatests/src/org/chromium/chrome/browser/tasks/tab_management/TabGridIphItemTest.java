// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.rotateDeviceToOrientation;

import android.content.res.Configuration;
import android.os.Build;
import android.support.test.espresso.NoMatchingRootException;
import android.support.test.espresso.UiController;
import android.support.test.espresso.ViewAction;
import android.support.test.filters.MediumTest;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.hamcrest.Matcher;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.test.util.UiRestriction;

/** End-to-end tests for TabGridIphItem component. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
public class TabGridIphItemTest {
    // clang-format on
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() {
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
        mActivityTestRule.startMainActivityFromLauncher();
        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        CriteriaHelper.pollUiThread(mActivityTestRule.getActivity()
                                            .getTabModelSelector()
                                            .getTabModelFilterProvider()
                                            .getCurrentTabModelFilter()::isTabModelRestored);
    }

    @Test
    @MediumTest
    public void testShowAndHideIphItem() throws InterruptedException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        verifyIphEntranceShowing(cta);

        // Enter the IPH dialog and exit by clicking "OK" button.
        enterIphDialog(cta);
        verifyIphDialogShowing(cta);
        exitIphDialogByClickingButton(cta);
        verifyIphDialogHiding(cta);

        // Exiting IPH dialog should not dismiss the IPH.
        verifyIphEntranceShowing(cta);

        // Enter the IPH dialog and exit by clicking ScrimView.
        enterIphDialog(cta);
        verifyIphDialogShowing(cta);
        exitIphDialogByClickingScrim(cta);
        verifyIphDialogHiding(cta);

        // Exiting IPH dialog should not dismiss the IPH.
        verifyIphEntranceShowing(cta);

        // Explicitly close IPH entrance should dismiss the IPH.
        closeIphEntrance(cta);
        verifyIphEntranceHiding(cta);
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.N_MR1, message = "https://crbug.com/1023430")
    public void testIphItemScreenRotation() throws InterruptedException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        verifyIphEntranceShowing(cta);
        enterIphDialog(cta);

        // Check the margins for default orientation which is portrait.
        assertEquals(Configuration.ORIENTATION_PORTRAIT,
                cta.getResources().getConfiguration().orientation);
        verifyDialogMargins(cta, Configuration.ORIENTATION_PORTRAIT);

        // Rotate the device to landscape mode.
        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        verifyDialogMargins(cta, Configuration.ORIENTATION_LANDSCAPE);
    }

    private void verifyIphEntranceShowing(ChromeTabbedActivity cta) {
        onView(withId(R.id.tab_grid_iph_item)).check((v, noMatchException) -> {
            if (noMatchException != null) throw noMatchException;

            Assert.assertTrue(v.findViewById(R.id.iph_description) instanceof TextView);
            Assert.assertTrue(v.findViewById(R.id.show_me_button) instanceof TextView);

            TextView descriptionText = v.findViewById(R.id.iph_description);
            String description = cta.getString(R.string.iph_drag_and_drop_introduction);
            assertEquals(description, descriptionText.getText());

            TextView showMeTextButton = v.findViewById(R.id.show_me_button);
            String buttonText = cta.getString(R.string.iph_drag_and_drop_show_me);
            assertEquals(buttonText, showMeTextButton.getText());
        });
    }

    private void verifyIphEntranceHiding(ChromeTabbedActivity cta) {
        onView(withId(R.id.tab_grid_iph_item)).check(doesNotExist());
    }

    private void verifyIphDialogShowing(ChromeTabbedActivity cta) {
        onView(withId(R.id.iph_dialog))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    String title = cta.getString(R.string.iph_drag_and_drop_title);
                    assertEquals(title, ((TextView) v.findViewById(R.id.title)).getText());

                    String description = cta.getString(R.string.iph_drag_and_drop_content);
                    assertEquals(
                            description, ((TextView) v.findViewById(R.id.description)).getText());

                    String closeButtonText = cta.getString(R.string.ok);
                    assertEquals(closeButtonText,
                            ((TextView) v.findViewById(R.id.close_button)).getText());
                });
    }

    private void verifyIphDialogHiding(ChromeTabbedActivity cta) {
        boolean isShowing = true;
        try {
            onView(withId(R.id.iph_dialog))
                    .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                    .check(matches(isDisplayed()));
        } catch (NoMatchingRootException e) {
            isShowing = false;
        } catch (Exception e) {
            assert false : "error when inspecting iph dialog.";
        }
        assertFalse(isShowing);
    }

    private void exitIphDialogByClickingButton(ChromeTabbedActivity cta) {
        onView(withId(R.id.close_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
    }

    private void exitIphDialogByClickingScrim(ChromeTabbedActivity cta) {
        onView(instanceOf(ScrimView.class))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(new ViewAction() {
                    @Override
                    public Matcher<View> getConstraints() {
                        return isDisplayed();
                    }

                    @Override
                    public String getDescription() {
                        return "click on ScrimView";
                    }

                    @Override
                    public void perform(UiController uiController, View view) {
                        assertTrue(view instanceof ScrimView);
                        ScrimView scrimView = (ScrimView) view;
                        scrimView.performClick();
                    }
                });
    }

    private void enterIphDialog(ChromeTabbedActivity cta) {
        assertTrue(cta.getLayoutManager().overviewVisible());
        onView(withId(R.id.show_me_button)).perform(click());
    }

    private void closeIphEntrance(ChromeTabbedActivity cta) {
        assertTrue(cta.getLayoutManager().overviewVisible());
        onView(withId(R.id.close_iph_button)).perform(click());
    }

    private void verifyDialogMargins(ChromeTabbedActivity cta, int orientation) {
        verifyIphDialogShowing(cta);
        DisplayMetrics displayMetrics = new DisplayMetrics();
        cta.getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
        int screenHeight = displayMetrics.heightPixels;

        int dialogHeight =
                (int) cta.getResources().getDimension(R.dimen.tab_grid_iph_dialog_height);
        int updatedDialogTopMargin = Math.max((screenHeight - dialogHeight) / 2,
                (int) cta.getResources().getDimension(R.dimen.tab_grid_iph_dialog_top_margin));
        int sideMargin =
                (int) cta.getResources().getDimension(R.dimen.tab_grid_iph_dialog_side_margin);
        int textTopMarginPortrait = (int) cta.getResources().getDimension(
                R.dimen.tab_grid_iph_dialog_text_top_margin_portrait);
        int textTopMarginLandscape = (int) cta.getResources().getDimension(
                R.dimen.tab_grid_iph_dialog_text_top_margin_landscape);
        int textSideMargin =
                (int) cta.getResources().getDimension(R.dimen.tab_grid_iph_dialog_text_side_margin);

        int dialogTopMargin;
        int dialogSideMargin;
        int textTopMargin;
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            dialogTopMargin = updatedDialogTopMargin;
            dialogSideMargin = sideMargin;
            textTopMargin = textTopMarginPortrait;
        } else {
            dialogTopMargin = sideMargin;
            dialogSideMargin = updatedDialogTopMargin;
            textTopMargin = textTopMarginLandscape;
        }

        onView(withId(R.id.iph_dialog))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    ViewGroup.MarginLayoutParams realMargins =
                            (ViewGroup.MarginLayoutParams) v.getLayoutParams();
                    assertEquals(dialogTopMargin, realMargins.topMargin);
                    assertEquals(dialogSideMargin, realMargins.leftMargin);
                });

        onView(withId(R.id.title))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    ViewGroup.MarginLayoutParams realMargins =
                            (ViewGroup.MarginLayoutParams) v.getLayoutParams();
                    assertEquals(textTopMargin, realMargins.topMargin);
                    assertEquals(textSideMargin, realMargins.leftMargin);
                });

        onView(withId(R.id.description))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    ViewGroup.MarginLayoutParams realMargins =
                            (ViewGroup.MarginLayoutParams) v.getLayoutParams();
                    assertEquals(0, realMargins.topMargin);
                    assertEquals(textTopMargin, realMargins.bottomMargin);
                    assertEquals(textSideMargin, realMargins.leftMargin);
                });

        onView(withId(R.id.close_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    ViewGroup.MarginLayoutParams realMargins =
                            (ViewGroup.MarginLayoutParams) v.getLayoutParams();
                    assertEquals(textTopMargin, realMargins.topMargin);
                    assertEquals(textSideMargin, realMargins.leftMargin);
                });
    }
}