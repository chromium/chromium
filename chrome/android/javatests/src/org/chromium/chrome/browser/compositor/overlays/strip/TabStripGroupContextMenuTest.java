// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.pressKey;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isFocused;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withParentIndex;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import androidx.test.espresso.ViewAssertion;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Instrumentation tests for tab strip group title long-press menu popup */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.ANDROID_THEME_MODULE,
})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511288697
public class TabStripGroupContextMenuTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private StripLayoutHelper mStripLayoutHelper;
    private Token mTabGroupId;
    private ModalDialogManager mModalDialogManager;
    private ChromeTabbedActivity mInitialRegularActivity;
    private CtaPageStation mPage;

    @Before
    public void setUp() throws Exception {
        mPage = mActivityTestRule.startOnBlankPage();
        mInitialRegularActivity = (ChromeTabbedActivity) mPage.getActivity();
        mStripLayoutHelper =
                TabStripTestUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        mModalDialogManager = mActivityTestRule.getActivity().getModalDialogManager();
    }

    @After
    public void tearDown() {
        // Dismiss any remaining context menu.
        ThreadUtils.runOnUiThreadBlocking(() -> mStripLayoutHelper.dismissContextMenu());

        // Dismiss any visible dialogs(crbug.com/394606261). Clicking anywhere to dismiss the popup
        // menu may unintentionally trigger a menu item (e.g. "Ungroup"), which can show a dialog.
        // Attempts to redirect the click to views e.g.(R.id.compositor_view_holder) didn't work, as
        // no views outside the popup menu were accessible while it was showing. Dismissing the
        // popup menu directly via StripLayoutHelper was also ineffective, so explicitly dismissing
        // all dialogs.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
                });
        mActivityTestRule.getActivityTestRule().setActivity(mInitialRegularActivity);
    }

    @Test
    @SmallTest
    public void testOpenNewTabInGroup() {
        // Prepare standard state and show menu.
        prepareStandardState();
        showMenu();

        // Assert there are 2 grouped tabs.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);
        int tabCount =
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabCountForGroup(mTabGroupId));
        assertEquals("There should be 2 tabs in group", 2, tabCount);

        // Verify and click "New tab in group".
        onView(withText(R.string.open_new_tab_in_group_context_menu_item))
                .check(matches(isDisplayed()));
        onView(withText(R.string.open_new_tab_in_group_context_menu_item)).perform(click());

        // Verify the grouped tab count is incremented.
        int finalTabCount =
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabCountForGroup(mTabGroupId));
        assertEquals("There should be 3 tabs in group", tabCount + 1, finalTabCount);
    }

    @Test
    @SmallTest
    public void testUngroup() {
        // Prepare standard state and show menu.
        prepareStandardState();
        showMenu();

        // Assert there are 2 grouped tabs.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);
        int tabCount =
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabCountForGroup(mTabGroupId));
        assertEquals("There should be 2 tabs in group", 2, tabCount);

        // Verify and click "Ungroup".
        onView(withText(R.string.ungroup_tab_group_menu_item)).check(matches(isDisplayed()));
        onView(withText(R.string.ungroup_tab_group_menu_item)).perform(click());

        // Verify confirmation dialog is showing and tab group is ungrouped after confirming the
        // action.
        verifyModalDialog(/* shouldShow= */ true);
        onView(withText(R.string.ungroup_tab_group_action)).perform(click());
        int finalTabCount =
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabCountForGroup(mTabGroupId));
        assertEquals("Tab group should be ungrouped", 0, finalTabCount);

        // Verify no tab group exists.
        for (int i = 0; i < getTabCountOnUiThread(tabModel); i++) {
            int j = i;
            boolean isTabInGroup =
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> tabModel.isTabInTabGroup(tabModel.getTabAt(j)));
            assertFalse("Tab should not be grouped", isTabInGroup);
        }
    }

    @Test
    @SmallTest
    public void testUngroup_Incognito() {
        // Prepare incognito state and show menu.
        prepareIncognitoState();
        showMenu();

        // Verify "Delete group" option should not be showing for incognito.
        onView(withText(R.string.tab_grid_dialog_toolbar_delete_group)).check(doesNotExist());

        // Assert there are 2 grouped tabs.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ true);
        int tabCount =
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabCountForGroup(mTabGroupId));
        assertEquals("There should be 2 tabs in group", 2, tabCount);

        // Verify and click "Ungroup".
        onView(withText(R.string.ungroup_tab_group_menu_item)).check(matches(isDisplayed()));
        onView(withText(R.string.ungroup_tab_group_menu_item)).perform(click());

        // Verify confirmation dialog is not showing for incognito and tab group is immediately
        // ungrouped.
        verifyModalDialog(/* shouldShow= */ false);
        int finalTabCount =
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabCountForGroup(mTabGroupId));
        assertEquals("Tab group should be ungrouped", 0, finalTabCount);

        // Verify no tab group exists.
        for (int i = 0; i < getTabCountOnUiThread(tabModel); i++) {
            int j = i;
            boolean isTabInGroup =
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> tabModel.isTabInTabGroup(tabModel.getTabAt(j)));
            assertFalse("Tab should not be grouped", isTabInGroup);
        }
    }

    @Test
    @SmallTest
    public void testCloseGroup() {
        // Prepare standard state and show menu.
        prepareStandardState();
        showMenu();

        // Assert there are 2 grouped tabs.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);
        int tabCount =
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabCountForGroup(mTabGroupId));
        assertEquals("There should be 2 tabs in group", 2, tabCount);

        // Assert last tab is an ungrouped tab.
        Tab ungroupedTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(2));
        assertFalse(
                "Last tab should not be grouped",
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.isTabInTabGroup(ungroupedTab)));

        // Verify and click "Close group".
        onView(withText(R.string.tab_grid_dialog_toolbar_close_group))
                .check(matches(isDisplayed()));
        onView(withText(R.string.tab_grid_dialog_toolbar_close_group)).perform(click());

        // Assert tab group is closed and undo option showed.
        assertFalse(
                "Tab group should be closed",
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.tabGroupExists(mTabGroupId)));
        assertEquals("Expected only one tab to be present", 1, getTabCountOnUiThread(tabModel));
        Tab firstTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(0));
        assertEquals(
                "Expected the only tab remain is the ungrouped tab",
                ungroupedTab.getId(),
                firstTab.getId());
        onView(withText("Undo")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testCloseGroup_Incognito() {
        // Prepare incognito state and show menu.
        prepareIncognitoState();
        showMenu();

        // Verify "Delete group" option should not be showing for incognito.
        onView(withText(R.string.tab_grid_dialog_toolbar_delete_group)).check(doesNotExist());

        // Assert there are 2 grouped tabs.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ true);
        int tabCount =
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabCountForGroup(mTabGroupId));
        assertEquals("There should be 2 tabs in group", 2, tabCount);

        // Assert last tab is an ungrouped tab.
        Tab ungroupedTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(2));
        assertFalse(
                "Last tab should not be grouped",
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.isTabInTabGroup(ungroupedTab)));

        // Verify and click "Close group".
        onView(withText(R.string.tab_grid_dialog_toolbar_close_group))
                .check(matches(isDisplayed()));
        onView(withText(R.string.tab_grid_dialog_toolbar_close_group)).perform(click());

        // Assert tab group is closed and undo option not showed.
        assertFalse(
                "Tab group should be closed",
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.tabGroupExists(mTabGroupId)));
        assertEquals("Expected only one tab to be present", 1, getTabCountOnUiThread(tabModel));
        Tab firstTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(0));
        assertEquals(
                "Expected the only tab remain is the ungrouped tab",
                ungroupedTab.getId(),
                firstTab.getId());
        onView(withText("Undo")).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testDeleteGroup() {
        // Prepare standard state and show menu.
        prepareStandardState();
        showMenu();

        // Assert there are 2 grouped tabs.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);
        int tabCount =
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabCountForGroup(mTabGroupId));
        assertEquals("There should be 2 tabs in group", 2, tabCount);

        // Assert last tab is an ungrouped tab.
        Tab ungroupedTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(2));
        assertFalse(
                "Last tab should not be grouped",
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.isTabInTabGroup(ungroupedTab)));

        // Verify and click "Delete group".
        onView(withText(R.string.tab_grid_dialog_toolbar_delete_group))
                .check(matches(isDisplayed()));
        onView(withText(R.string.tab_grid_dialog_toolbar_delete_group)).perform(click());

        // Verify confirmation dialog is showing and tab group is deleted after confirming the
        // action.
        verifyModalDialog(/* shouldShow= */ true);
        onView(withText(R.string.delete_tab_group_action)).perform(click());
        assertFalse(
                "Tab group should be deleted",
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.tabGroupExists(mTabGroupId)));
        assertEquals("Expected only one tab to be present", 1, getTabCountOnUiThread(tabModel));
        Tab firstTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(0));
        assertEquals(
                "Expected the only tab remain is the ungrouped tab",
                ungroupedTab.getId(),
                firstTab.getId());
    }

    @Test
    @SmallTest
    public void testUpdateAndDeleteGroupTitle() {
        // Prepare standard state and show menu.
        prepareStandardState();
        showMenu();

        // Verify default group title.
        String title = "2 tabs";
        onView(withText(title)).check(matches(isDisplayed()));

        // Update tab group title and verify.
        title = "newTitle";
        updateGroupTitle(title);
        showMenu();
        onView(withText(title)).check(matches(isDisplayed()));

        // Delete the group title by clearing the edit box and verify its default to "N tabs".
        title = "";
        updateGroupTitle(title);
        showMenu();
        onView(withText("2 tabs")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testUpdateGroupColor() {
        // Prepare standard state and show menu.
        prepareStandardState();
        showMenu();
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);

        // Verify the default grey color is selected.
        @TabGroupColorId
        int color = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabGroupColor(mTabGroupId));
        assertEquals("The default grey color should be selected", TabGroupColorId.GREY, color);

        // Select the blue color.
        String blueColor = mActivityTestRule.getActivity().getString(R.string.tab_group_color_blue);
        String notSelectedStringBlue =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string
                                        .accessibility_tab_group_color_picker_color_item_not_selected_description,
                                blueColor);
        onView(withContentDescription(notSelectedStringBlue)).perform(click());

        // Verify the blue color is selected.
        color = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabGroupColor(mTabGroupId));
        assertEquals("The blue color should be selected", TabGroupColorId.BLUE, color);
    }

    private static ViewAssertion matchesWithMessage(Matcher<? super View> matcher, String message) {
        return (view, noViewFoundException) -> {
            if (noViewFoundException != null) {
                throw new AssertionError(message, noViewFoundException);
            }
            assertThat(message, view, matcher);
        };
    }

    @Test
    @SmallTest
    @Feature("KeyboardA11y")
    public void testKeyboardFocusAndActivation() {
        // Prepare standard state and show menu.
        prepareStandardState();
        int numTabsBeforeClick =
                getTabCountOnUiThread(mActivityTestRule.getActivity().getCurrentTabModel());
        showMenu();

        // Start with the edit text box. Click to focus, then hit down arrow.
        onView(withId(R.id.tab_group_title)).perform(click());
        onView(withId(R.id.tab_group_title)).perform(pressKey(KeyEvent.KEYCODE_DPAD_DOWN));

        // One of the color picker circles should be focused in the first row.
        onView(allOf(isDescendantOfA(withId(R.id.color_picker_first_row)), isFocused()))
                .check(
                        matchesWithMessage(
                                isDisplayed(),
                                "One of the color picker circles in the first row should be"
                                        + " focused."));

        // Check if there's a second row of colors and navigate accordingly.
        final boolean[] hasSecondRow = new boolean[1];
        onView(withId(R.id.color_picker_second_row))
                .check(
                        (view, noViewFoundException) -> {
                            if (view instanceof ViewGroup group) {
                                hasSecondRow[0] = group.getChildCount() > 0;
                            }
                        });

        if (hasSecondRow[0]) {
            // Hit down arrow to move from the first row to the second row of the color picker.
            onView(isFocused()).perform(pressKey(KeyEvent.KEYCODE_DPAD_DOWN));
            onView(allOf(isDescendantOfA(withId(R.id.color_picker_second_row)), isFocused()))
                    .check(
                            matchesWithMessage(
                                    isDisplayed(),
                                    "One of the color picker circles in the second row should be"
                                            + " focused."));
        }

        // Hit down arrow to move from the color picker to the action menu list.
        onView(isFocused()).perform(pressKey(KeyEvent.KEYCODE_DPAD_DOWN));

        // The second element of tab_group_action_menu_list should be focused (skip divider).
        onView(allOf(withParent(withId(R.id.tab_group_action_menu_list)), withParentIndex(1)))
                .check(
                        matchesWithMessage(
                                isFocused(),
                                "The second element of tab_group_action_menu_list should be"
                                        + " focused."));

        // Now hit the button.
        onView(isFocused()).perform(pressKey(KeyEvent.KEYCODE_SPACE));

        assertEquals(
                "Tab count should increase by 1 after activating 'New tab in group'.",
                numTabsBeforeClick + 1,
                getTabCountOnUiThread(mActivityTestRule.getActivity().getCurrentTabModel()));
    }

    @Test
    @SmallTest
    public void testSubMenuScrollability() throws InterruptedException {
        // Specifically test the drill-down case.
        HierarchicalMenuController.setDrillDownOverrideValueForTesting(true);

        // Open a new window so we'll get a submenu for "Move to another window"
        mPage = mActivityTestRule.startOnBlankPage().openNewWindowFast();

        // Switch the "main" activity to the newly-opened window, which is now focused.
        mActivityTestRule.getActivityTestRule().setActivity(mPage.getActivity());

        BaseMatcher<View> isScrollContainerMatcher =
                new BaseMatcher<>() {
                    @Override
                    public void describeTo(Description description) {
                        description.appendText("isScrollContainer");
                    }

                    @Override
                    public boolean matches(Object o) {
                        if (o instanceof ListView listView) {
                            return listView.isScrollContainer();
                        }
                        return false;
                    }

                    @Override
                    public void describeMismatch(Object o, Description description) {
                        description.appendText(
                                "Expected "
                                        + o
                                        + " to be a listView where isScrollContainer was true");
                    }
                };

        BaseMatcher<View> layoutParamsMatcher =
                new BaseMatcher<>() {
                    @Override
                    public void describeTo(Description description) {
                        description.appendText("has no excess space");
                    }

                    @Override
                    public boolean matches(Object o) {
                        if (o instanceof ListView listView) {
                            View lastChild = listView.getChildAt(listView.getChildCount() - 1);
                            return listView.getHeight()
                                            - listView.getPaddingBottom()
                                            - (lastChild.getY() + lastChild.getHeight())
                                    < 10;
                        }
                        return false;
                    }

                    @Override
                    public void describeMismatch(Object o, Description description) {
                        description.appendText(
                                "Expected "
                                        + o
                                        + " to be a ListView without excess space at the bottom,"
                                        + " but ");
                        if (o instanceof ListView listView) {
                            View lastChild = listView.getChildAt(listView.getChildCount() - 1);
                            description.appendText(
                                    "the height of the ListView (minus padding) was at "
                                            + (listView.getHeight() - listView.getPaddingBottom())
                                            + " and its last child's bottom was at "
                                            + (lastChild.getY() + lastChild.getHeight())
                                            + ", I think the last child is = "
                                            + lastChild);
                        } else {
                            description.appendText(o + " was not a ListView");
                        }
                    }
                };

        // Set up the test as usual.
        prepareStandardState();

        // Show the menu on the tab group now that it is in a new window.
        showMenu();

        // Get the ListView of the context menu.
        onView(withId(R.id.tab_group_action_menu_list))
                .check(matches(not(isScrollContainerMatcher)));

        String moveToAnotherWindow =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(
                                R.plurals.move_group_to_another_window_context_menu_item, 2);

        // Click on "Move tab to other window" to open a submenu.
        onView(withText(moveToAnotherWindow)).perform(click());

        // Sub-menu should be scrollable and should not have excess space.
        onView(withId(R.id.tab_group_action_menu_list)).check(matches(isScrollContainerMatcher));
        onView(withId(R.id.tab_group_action_menu_list)).check(matches(layoutParamsMatcher));

        // Go back to the main menu.
        onView(withText(moveToAnotherWindow)).perform(click());

        // Main menu should not be scrollable again.
        onView(withId(R.id.tab_group_action_menu_list))
                .check(matches(not(isScrollContainerMatcher)));
    }

    private void prepareStandardState() {
        // mPage is initialized in setUp() or updated by the test.
        mPage = mPage.openNewTabFast().loadAboutBlank();
        mPage = mPage.openNewTabFast().loadAboutBlank();

        ChromeTabbedActivity activity = (ChromeTabbedActivity) mPage.getActivity();
        // Wait for the activity to be fully initialized before accessing TabModelSelector.
        CriteriaHelper.pollUiThread(
                () -> {
                    try {
                        return activity.getTabModelSelector() != null;
                    } catch (IllegalStateException e) {
                        return false;
                    }
                });

        TabStripTestUtils.createTabGroup(
                activity, /* isIncognito= */ false, /* firstIndex= */ 0, /* secondIndex= */ 1);

        // Re-initialize helper and manager for the current activity.
        if (mInitialRegularActivity.getTabModelSelector().isIncognitoBrandedModelSelected()) {
            mStripLayoutHelper = TabStripTestUtils.getActiveStripLayoutHelper(activity);
            mModalDialogManager = activity.getModalDialogManager();
        }
    }

    private void prepareIncognitoState() {
        // Transition to incognito from the current regular page.
        mPage = mPage.openNewIncognitoTabOrWindowFast();
        mPage = mPage.openNewIncognitoTabFast();
        mPage = mPage.openNewIncognitoTabFast();

        ChromeTabbedActivity activity = (ChromeTabbedActivity) mPage.getActivity();
        // If we opened a new window, ensure the rule tracks it for cleanup.
        mActivityTestRule.getActivityTestRule().setActivity(activity);

        TabStripTestUtils.createTabGroup(
                activity, /* isIncognito= */ true, /* firstIndex= */ 0, /* secondIndex= */ 1);

        mStripLayoutHelper = TabStripTestUtils.getActiveStripLayoutHelper(activity);
        mModalDialogManager = activity.getModalDialogManager();
    }

    private void verifyModalDialog(boolean shouldShow) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(shouldShow));
                });
    }

    private void showMenu() {
        mStripLayoutHelper =
                TabStripTestUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        StripLayoutView[] views = mStripLayoutHelper.getStripLayoutViewsForTesting();
        assertTrue(
                "First view should be a group title.", views[0] instanceof StripLayoutGroupTitle);
        StripLayoutGroupTitle stripLayoutGroupTitle = ((StripLayoutGroupTitle) views[0]);
        float x = stripLayoutGroupTitle.getPaddedX();
        float y = stripLayoutGroupTitle.getPaddedY();
        mTabGroupId = stripLayoutGroupTitle.getTabGroupId();

        final StripLayoutHelperManager manager =
                mActivityTestRule.getActivity().getLayoutManager().getStripLayoutHelperManager();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
                            @Override
                            public void run() {
                                manager.simulateLongPress(x, y);
                            }
                        });
        onViewWaiting(allOf(withId(R.id.tab_group_action_menu_list), isDisplayed()));
    }

    private void updateGroupTitle(String title) {
        KeyboardVisibilityDelegate delegate =
                mActivityTestRule.getActivity().getWindowAndroid().getKeyboardDelegate();

        // Click group title text box to display keyboard for editing.
        onView(withId(R.id.tab_group_title)).perform(click());

        // Verify keyboard is displayed.
        CriteriaHelper.pollUiThread(
                () ->
                        delegate.isKeyboardShowing(
                                mActivityTestRule
                                        .getActivity()
                                        .getCompositorViewHolderForTesting()));

        // Enter new title in text box and press "enter" to dismiss keyboard to update group title.
        onView(withId(R.id.tab_group_title))
                .perform(replaceText(title))
                .perform(pressImeActionButton());

        // Verify keyboard is dismissed.
        CriteriaHelper.pollUiThread(
                () ->
                        !delegate.isKeyboardShowing(
                                mActivityTestRule
                                        .getActivity()
                                        .getCompositorViewHolderForTesting()));
    }
}
