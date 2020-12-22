// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.AllOf.allOf;

import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.test.util.UiRestriction;

/**
 * End-to-end tests for TabSuggestion.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID+"<Study",
        ChromeFeatureList.TAB_GROUPS_ANDROID,
        ChromeFeatureList.CLOSE_TAB_SUGGESTIONS+"<Study"})
// Disable TAB_TO_GTS_ANIMATION to make it less flaky. When animation is enabled, the suggestion
// cards will be removed temporarily, then append again.
@Features.DisableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "force-fieldtrials=Study/Group"})
@FlakyTest(message = "https://crbug.com/1161272")
public class TabSuggestionMessageCardTest {
    // clang-format on
    private static final String BASE_PARAMS = "force-fieldtrial-params="
            + "Study.Group:baseline_tab_suggestions/true/enable_launch_polish/true";
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private final TabSelectionEditorTestingRobot mTabSelectionEditorTestingRobot =
            new TabSelectionEditorTestingRobot();
    private final String mClosingSuggestionMessage =
            "3 of your tabs haven't been used lately. Close them?";
    private final String mGroupingSuggestionMessage = "3 tabs seem related. Group them?";

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, 3, 0, "about:blank");
    }

    private void enteringTabSwitcherAndVerifySuggestionIsShown(String suggestionText) {
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(allOf(withParent(withId(R.id.tab_grid_message_item)), withText(suggestionText)))
                .check(matches(isDisplayed()));
    }

    private void reviewSuggestion() {
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        mTabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsVisible();
    }

    private void acceptSuggestion() {
        mTabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsVisible();
        mTabSelectionEditorTestingRobot.actionRobot.clickToolbarActionButton();
        mTabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsHidden();
    }

    private void dismissSuggestion(boolean isReviewed) {
        if (isReviewed) {
            mTabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsVisible();
            mTabSelectionEditorTestingRobot.actionRobot.clickToolbarNavigationButton();
            mTabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        } else {
            onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
            onView(allOf(withId(R.id.close_button), withParent(withId(R.id.tab_grid_message_item))))
                    .perform(click());
        }
    }

    @Test
    @MediumTest
    @FlakyTest(message = "crbug.com/1075650")
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_close_tab_suggestions/true"})
    public void closeTabSuggestionReviewedAndAccepted() {
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        enteringTabSwitcherAndVerifySuggestionIsShown(mClosingSuggestionMessage);
        reviewSuggestion();
        acceptSuggestion();

        onViewWaiting(allOf(withParent(withId(R.id.snackbar)), withText("3 tabs closed")));
    }

    @Test
    @MediumTest
    @CommandLineFlags.
    Add({BASE_PARAMS + "/baseline_close_tab_suggestions/true/min_time_between_prefetches/0"})
    @DisableIf.Build(sdk_is_less_than = VERSION_CODES.N, message = "https://crbug.com/1095535")
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1095535")
    public void closeTabSuggestionReviewedAndDismissed() {
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        enteringTabSwitcherAndVerifySuggestionIsShown(mClosingSuggestionMessage);
        reviewSuggestion();
        dismissSuggestion(true);

        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @CommandLineFlags.
    Add({BASE_PARAMS + "/baseline_group_tab_suggestions/true/min_time_between_prefetches/0"})
    public void groupTabSuggestionReviewedAndAccepted() {
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        enteringTabSwitcherAndVerifySuggestionIsShown(mGroupingSuggestionMessage);
        reviewSuggestion();
        acceptSuggestion();

        onViewWaiting(allOf(withParent(withId(R.id.snackbar)), withText("3 tabs grouped")));
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS +
        "/baseline_group_tab_suggestions/true/min_time_between_prefetches/0"})
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1102423")
    public void groupTabSuggestionReviewedAndDismissed() {
        // clang-format on
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        enteringTabSwitcherAndVerifySuggestionIsShown(mGroupingSuggestionMessage);
        reviewSuggestion();
        dismissSuggestion(true);

        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_group_tab_suggestions/true" +
            "/baseline_close_tab_suggestions/true"})
    @DisabledTest(message = "crbug.com/1085452 Enable this test and remove the one below if the" +
            "bug is resolved")
    public void groupAndCloseTabSuggestionDismissedAndShowNext() {
        // clang-format on
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        enteringTabSwitcherAndVerifySuggestionIsShown(mGroupingSuggestionMessage);
        dismissSuggestion(false);
        onView(allOf(withParent(withId(R.id.tab_grid_message_item)),
                       withText(mClosingSuggestionMessage)))
                .check(matches(isDisplayed()));
        dismissSuggestion(false);

        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1085452")
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_group_tab_suggestions/true" +
            "/baseline_close_tab_suggestions/true/min_time_between_prefetches/0"})
    public void groupAndCloseTabSuggestionDismissedAndShowNext_temp() {
        // clang-format on
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        dismissSuggestion(false);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        dismissSuggestion(false);
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_group_tab_suggestions/true" +
            "/baseline_close_tab_suggestions/true"})
    @DisabledTest(message = "crbug.com/1085452 Enable this test if the bug is resolved")
    public void groupAndCloseTabSuggestionReviewDismissedAndShowNext() {
        // clang-format on
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        enteringTabSwitcherAndVerifySuggestionIsShown(mGroupingSuggestionMessage);
        reviewSuggestion();
        dismissSuggestion(true);
        onView(allOf(withParent(withId(R.id.tab_grid_message_item)),
                       withText(mClosingSuggestionMessage)))
                .check(matches(isDisplayed()));
        reviewSuggestion();
        dismissSuggestion(true);

        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_group_tab_suggestions/true" +
            "/baseline_close_tab_suggestions/true"})
    @DisabledTest(message = "crbug.com/1085452 Enable this test if the bug is resolved")
    public void groupAndCloseTabSuggestionAccepted() {
        // clang-format on
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        enteringTabSwitcherAndVerifySuggestionIsShown(mGroupingSuggestionMessage);
        reviewSuggestion();
        acceptSuggestion();

        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }
}
