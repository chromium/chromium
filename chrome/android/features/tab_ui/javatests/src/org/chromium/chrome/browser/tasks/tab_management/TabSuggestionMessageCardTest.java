// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.core.AllOf.allOf;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * End-to-end tests for TabSuggestion.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures({ChromeFeatureList.CLOSE_TAB_SUGGESTIONS+"<Study"})
// Disable TAB_TO_GTS_ANIMATION to make it less flaky. When animation is enabled, the suggestion
// cards will be removed temporarily, then append again.
// TODO(https://crbug.com/1362059): The message cards aren't shown the first time when entering GTS
// with Start surface enabled.
@DisableFeatures({
    ChromeFeatureList.TAB_TO_GTS_ANIMATION, ChromeFeatureList.START_SURFACE_ANDROID})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "force-fieldtrials=Study/Group"})
public class TabSuggestionMessageCardTest {
    // clang-format on
    private static final String BASE_PARAMS = "force-fieldtrial-params="
            + "Study.Group:baseline_tab_suggestions/true/enable_launch_polish/true"
            + "/min_time_between_prefetches/0/thumbnail_aspect_ratio/1.0";
    private static final String ENABLE_CLOSE_SUGGESTION_PARAM =
            "/baseline_close_tab_suggestions/true";
    private static final String ENABLE_GROUP_SUGGESTION_PARAM =
            "/baseline_group_tab_suggestions/true";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private final TabSelectionEditorTestingRobot mTabSelectionEditorTestingRobot =
            new TabSelectionEditorTestingRobot();
    private final String mClosingSuggestionMessage =
            "3 of your tabs haven't been used lately. Close them?";
    private final String mGroupingSuggestionMessage = "3 tabs seem related. Group them?";

    private void createBlankBackgroundTabs(int numTabs) {
        for (int i = 0; i < numTabs; i++) {
            mActivityTestRule.getActivity().getTabCreator(false).createNewTab(
                    new LoadUrlParams("about:blank"), TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                    null);
        }
    }

    private void createBlankForegroundTabs(int numTabs) {
        for (int i = 0; i < numTabs; i++) {
            mActivityTestRule.getActivity().getTabCreator(false).createNewTab(
                    new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
        }
    }

    private CallbackHelper mPaintedCallback = new CallbackHelper();

    @Before
    public void setUp() throws ExecutionException {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> new TabModelSelectorTabObserver(
                                mActivityTestRule.getActivity().getTabModelSelector()) {
                    @Override
                    public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                        mPaintedCallback.notifyCalled();
                    }
                });

        // TabObserver#didFirstVisuallyNonEmptyPaint will invalidate and fetch for new suggestion.
        // Create one foreground tab and one background tab to ensure mPaintedCallback only call
        // once to make the tests less flaky.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> createBlankForegroundTabs(1));
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> createBlankBackgroundTabs(1));
        assertThat("TabModelSelector should have total of 3 tabs",
                mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount(), is(3));

        try {
            mPaintedCallback.waitForCallback(0);
        } catch (TimeoutException ex) {
            Assert.fail("Never received tab painted event");
        }
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

    private void acceptSuggestion(int id) {
        mTabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsVisible();
        mTabSelectionEditorTestingRobot.actionRobot.clickToolbarActionView(id);
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
    @DisabledTest(message = "https://crbug.com/1441919")
    @CommandLineFlags.Add({BASE_PARAMS + ENABLE_CLOSE_SUGGESTION_PARAM})
    public void closeTabSuggestionReviewedAndAccepted() {
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        enteringTabSwitcherAndVerifySuggestionIsShown(mClosingSuggestionMessage);
        reviewSuggestion();
        acceptSuggestion(R.id.tab_selection_editor_close_menu_item);

        onViewWaiting(allOf(withParent(withId(R.id.snackbar)), withText("3 tabs closed")));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1458843")
    @CommandLineFlags.Add({BASE_PARAMS + ENABLE_CLOSE_SUGGESTION_PARAM})
    public void closeTabSuggestionReviewedAndDismissed() {
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        enteringTabSwitcherAndVerifySuggestionIsShown(mClosingSuggestionMessage);
        reviewSuggestion();
        dismissSuggestion(true);

        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + ENABLE_GROUP_SUGGESTION_PARAM})
    @DisabledTest(message = "Flaky, see crbug.com/1469393")
    public void groupTabSuggestionReviewedAndAccepted() {
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        enteringTabSwitcherAndVerifySuggestionIsShown(mGroupingSuggestionMessage);
        reviewSuggestion();
        acceptSuggestion(R.id.tab_selection_editor_group_menu_item);

        onViewWaiting(allOf(withParent(withId(R.id.snackbar)), withText("3 tabs grouped")));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + ENABLE_GROUP_SUGGESTION_PARAM})
    @DisabledTest(message = "crbug.com/1257781")
    public void groupTabSuggestionReviewedAndDismissed() {
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        enteringTabSwitcherAndVerifySuggestionIsShown(mGroupingSuggestionMessage);
        reviewSuggestion();
        dismissSuggestion(true);

        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + ENABLE_GROUP_SUGGESTION_PARAM +
            ENABLE_CLOSE_SUGGESTION_PARAM})
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
    @CommandLineFlags.
    Add({BASE_PARAMS + ENABLE_GROUP_SUGGESTION_PARAM + ENABLE_CLOSE_SUGGESTION_PARAM})
    public void groupAndCloseTabSuggestionDismissedAndShowNext_temp() {
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
    @CommandLineFlags.
    Add({BASE_PARAMS + ENABLE_GROUP_SUGGESTION_PARAM + ENABLE_CLOSE_SUGGESTION_PARAM})
    @DisabledTest(message = "crbug.com/1085452 Enable this test if the bug is resolved")
    public void groupAndCloseTabSuggestionReviewDismissedAndShowNext() {
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
    @CommandLineFlags.
    Add({BASE_PARAMS + ENABLE_GROUP_SUGGESTION_PARAM + ENABLE_CLOSE_SUGGESTION_PARAM})
    @DisabledTest(message = "crbug.com/1085452 Enable this test if the bug is resolved")
    public void groupAndCloseTabSuggestionAccepted() {
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);

        enteringTabSwitcherAndVerifySuggestionIsShown(mGroupingSuggestionMessage);
        reviewSuggestion();
        acceptSuggestion(R.id.tab_selection_editor_group_menu_item);

        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }
}
