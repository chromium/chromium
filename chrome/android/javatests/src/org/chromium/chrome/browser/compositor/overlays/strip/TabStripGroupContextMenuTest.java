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
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
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

import static org.chromium.base.test.transit.Condition.whether;
import static org.chromium.base.test.transit.SimpleConditions.uiThreadCondition;
import static org.chromium.base.test.transit.ViewFinder.expectNoView;
import static org.chromium.base.test.transit.ViewFinder.waitForView;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import android.view.KeyEvent;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;

import androidx.annotation.StringRes;
import androidx.test.espresso.ViewAssertion;
import androidx.test.filters.SmallTest;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.color_picker.TabGroupColorPickerContainer;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.tab_strip.StripGroupTitleFacility;
import org.chromium.chrome.test.transit.tab_strip.TabStripFacility;
import org.chromium.chrome.test.transit.tab_strip.TabStripGroupContextMenuFacility;
import org.chromium.chrome.test.transit.tabmodel.TabGroupCreatedCondition;
import org.chromium.chrome.test.transit.ui.ModalDialogFacility;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupId;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Instrumentation tests for tab strip group title long-press menu popup */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@DisableFeatures(TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS)
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@DisableIf.Device(DeviceFormFactor.AUTOMOTIVE) // crbug.com/462592111
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
        ModalDialogView.disableButtonTapProtectionForTesting();
        mPage = mActivityTestRule.startOnBlankPage();
        mInitialRegularActivity = (ChromeTabbedActivity) mPage.getActivity();
        mStripLayoutHelper =
                TabStripTestUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        mModalDialogManager = mActivityTestRule.getActivity().getModalDialogManager();
    }

    @After
    public void tearDown() {
        // Dismiss any remaining context menu and visible dialogs (crbug.com/394606261), then wait
        // for the window to regain focus before resetting to the initial activity.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mStripLayoutHelper.dismissContextMenu();
                    mModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
                });
        CriteriaHelper.pollUiThread(() -> mActivityTestRule.getActivity().hasWindowFocus());
        mActivityTestRule.getActivityTestRule().setActivity(mInitialRegularActivity);
    }

    @Test
    @SmallTest
    public void testOpenNewTabInGroup() {
        // Prepare standard state and show menu.
        var menu = prepareStandardState().openContextMenu();

        // Assert there are 2 grouped tabs.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);
        int tabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                tabModel.getTabCountForGroup(
                                        TabGroupId.fromOptionalToken(mTabGroupId)));
        assertEquals("There should be 2 tabs in group", 2, tabCount);

        // Verify and click "New tab in group".
        mPage =
                menu.openNewTabInGroupTo()
                        .arriveAt(
                                RegularNewTabPageStation.newBuilder().initOpeningNewTab().build());

        // Verify the grouped tab count is incremented.
        int finalTabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                tabModel.getTabCountForGroup(
                                        TabGroupId.fromOptionalToken(mTabGroupId)));
        assertEquals("There should be 3 tabs in group", tabCount + 1, finalTabCount);
    }

    @Test
    @SmallTest
    public void testUngroup() {
        // Prepare standard state and show menu.
        var menu = prepareStandardState().openContextMenu();

        // Assert there are 2 grouped tabs.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);
        int tabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                tabModel.getTabCountForGroup(
                                        TabGroupId.fromOptionalToken(mTabGroupId)));
        assertEquals("There should be 2 tabs in group", 2, tabCount);

        // Click "Ungroup", confirm the dialog, and wait for the group to be ungrouped.
        confirmActionDialog(menu.ungroupTo(), R.string.ungroup_tab_group_action, tabModel);
        int finalTabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                tabModel.getTabCountForGroup(
                                        TabGroupId.fromOptionalToken(mTabGroupId)));
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
        var menu = prepareIncognitoState().openContextMenu();

        // Verify "Delete group" is absent in incognito.
        expectNoView(withText(R.string.tab_grid_dialog_toolbar_delete_group));

        // Assert there are 2 grouped tabs.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ true);
        int tabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                tabModel.getTabCountForGroup(
                                        TabGroupId.fromOptionalToken(mTabGroupId)));
        assertEquals("There should be 2 tabs in group", 2, tabCount);

        // Click "Ungroup" and wait for the group to be ungrouped without a confirmation dialog.
        menu.ungroupTo().waitFor(tabGroupRemoved(tabModel));
        assertFalse(
                "No confirmation dialog in incognito",
                ThreadUtils.runOnUiThreadBlocking(mModalDialogManager::isShowing));
        int finalTabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                tabModel.getTabCountForGroup(
                                        TabGroupId.fromOptionalToken(mTabGroupId)));
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
        var menu = prepareStandardState().openContextMenu();

        // Assert there are 2 grouped tabs.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);
        int tabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                tabModel.getTabCountForGroup(
                                        TabGroupId.fromOptionalToken(mTabGroupId)));
        assertEquals("There should be 2 tabs in group", 2, tabCount);

        // Assert last tab is an ungrouped tab.
        Tab ungroupedTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(2));
        assertFalse(
                "Last tab should not be grouped",
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.isTabInTabGroup(ungroupedTab)));

        // Verify and click "Close group".
        menu.closeGroupTo().waitFor(tabGroupRemoved(tabModel));

        // Assert tab group is closed and undo option showed.
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
        var menu = prepareIncognitoState().openContextMenu();

        // Verify "Delete group" is absent in incognito.
        expectNoView(withText(R.string.tab_grid_dialog_toolbar_delete_group));

        // Assert there are 2 grouped tabs.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ true);
        int tabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                tabModel.getTabCountForGroup(
                                        TabGroupId.fromOptionalToken(mTabGroupId)));
        assertEquals("There should be 2 tabs in group", 2, tabCount);

        // Assert last tab is an ungrouped tab.
        Tab ungroupedTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(2));
        assertFalse(
                "Last tab should not be grouped",
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.isTabInTabGroup(ungroupedTab)));

        // Verify and click "Close group".
        menu.closeGroupTo().waitFor(tabGroupRemoved(tabModel));

        // Assert tab group is closed and undo option not showed.
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
        var menu = prepareStandardState().openContextMenu();

        // Assert there are 2 grouped tabs.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);
        int tabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                tabModel.getTabCountForGroup(
                                        TabGroupId.fromOptionalToken(mTabGroupId)));
        assertEquals("There should be 2 tabs in group", 2, tabCount);

        // Assert last tab is an ungrouped tab.
        Tab ungroupedTab = ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getTabAt(2));
        assertFalse(
                "Last tab should not be grouped",
                ThreadUtils.runOnUiThreadBlocking(() -> tabModel.isTabInTabGroup(ungroupedTab)));

        // Click "Delete group", confirm the dialog, and wait for the group to be deleted.
        confirmActionDialog(menu.deleteGroupTo(), R.string.delete_tab_group_action, tabModel);
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
        var groupTitle = prepareStandardState();
        var menu = groupTitle.openContextMenu();

        // Verify default group title.
        String title = "2 tabs";
        onView(withText(title)).check(matches(isDisplayed()));

        // Update tab group title and verify.
        title = "newTitle";
        updateGroupTitle(groupTitle, menu, title);
        menu = groupTitle.openContextMenu();
        onView(withText(title)).check(matches(isDisplayed()));

        // Delete the group title by clearing the edit box and verify its default to "N tabs".
        title = "";
        updateGroupTitle(groupTitle, menu, title);
        groupTitle.openContextMenu();
        onView(withText("2 tabs")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CONTEXT_MENU_LIVE_TITLE_UPDATE)
    public void testLiveUpdateGroupTitle() {
        // Prepare standard state and show menu.
        var menu = prepareStandardState().openContextMenu();

        // Verify default group title in the menu.
        onView(withText("2 tabs")).check(matches(isDisplayed()));

        // Focus title edit box and replace text with rapid typing to verify debouncing.
        EditText titleEditText = focusTitleEditText(menu);

        // While the menu is STILL SHOWING (no enter pressed, no dismissal), verify the TabModel
        // updates live after the debounce period.
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);
        menu.runOnUiThreadTo(
                        () -> {
                            titleEditText.setText("live");
                            titleEditText.setText("liveTitle");
                        })
                .waitFor(
                        uiThreadCondition(
                                "Tab group title should be updated live while menu is open",
                                () ->
                                        whether(
                                                "liveTitle"
                                                        .equals(
                                                                tabModel.getTabGroupTitle(
                                                                        TabGroupId.fromRawToken(
                                                                                mTabGroupId))))));

        // Verify the menu is still open and displaying the new title.
        menu.titleElement.check(matches(withText("liveTitle")));

        // Clear the title in the edit box.
        // While still showing, verify that deleting the text clears the title in TabModel.
        menu.titleElement
                .performViewActionTo(replaceText(""))
                .waitFor(
                        uiThreadCondition(
                                "Tab group title in TabModel should be cleared while edit text is"
                                        + " empty",
                                () -> {
                                    String currentTitle =
                                            tabModel.getTabGroupTitle(
                                                    TabGroupId.fromRawToken(mTabGroupId));
                                    return whether(currentTitle == null || currentTitle.isEmpty());
                                }));

        // Verify that the EditText itself remains empty while the user is actively typing.
        menu.titleElement.check(matches(withText("")));
    }

    @Test
    @SmallTest
    public void testUpdateGroupColor() {
        // Prepare standard state and show menu.
        prepareStandardState().openContextMenu();
        TabModel tabModel =
                TabStripTestUtils.getTabModel(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);

        // Verify the default grey color is selected.
        @TabGroupColorId
        int color =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> tabModel.getTabGroupColor(TabGroupId.fromRawToken(mTabGroupId)));
        assertEquals("The default grey color should be selected", TabGroupColorId.GREY, color);

        // Select the blue color.
        String blueColor = mActivityTestRule.getActivity().getString(R.string.tab_group_color_blue);
        onView(withContentDescription(blueColor)).perform(click());

        // Verify the blue color is selected.
        color =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> tabModel.getTabGroupColor(TabGroupId.fromRawToken(mTabGroupId)));
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
        var groupTitle = prepareStandardState();
        int numTabsBeforeClick =
                getTabCountOnUiThread(mActivityTestRule.getActivity().getCurrentTabModel());
        groupTitle.openContextMenu();

        // Start with the edit text box. Click to focus, then hit down arrow.
        onView(withId(R.id.tab_group_title)).perform(click());
        waitForView(withId(R.id.tab_group_title), isFocused());
        onView(withId(R.id.tab_group_title)).perform(pressKey(KeyEvent.KEYCODE_DPAD_DOWN));

        // One of the color picker circles should be focused in the first row.
        onView(isFocused())
                .check(
                        (view, noViewFoundException) -> {
                            View parent = (View) view.getParent();
                            assertTrue(parent instanceof TabGroupColorPickerContainer);
                            TabGroupColorPickerContainer container =
                                    (TabGroupColorPickerContainer) parent;
                            float firstY = container.getChildAt(0).getY();
                            assertEquals(
                                    "Focused view should be in first row",
                                    firstY,
                                    view.getY(),
                                    0.1);
                        });

        // Check if there's a second row of colors and navigate accordingly.
        final boolean[] hasSecondRow = new boolean[1];
        onView(isAssignableFrom(TabGroupColorPickerContainer.class))
                .check(
                        (view, noViewFoundException) -> {
                            if (view instanceof TabGroupColorPickerContainer container) {
                                if (container.getChildCount() > 0) {
                                    float firstY = container.getChildAt(0).getY();
                                    for (int i = 1; i < container.getChildCount(); i++) {
                                        if (container.getChildAt(i).getY() > firstY) {
                                            hasSecondRow[0] = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        });

        if (hasSecondRow[0]) {
            // Hit down arrow to move from the first row to the second row of the color picker.
            onView(isFocused()).perform(pressKey(KeyEvent.KEYCODE_DPAD_DOWN));
            onView(isFocused())
                    .check(
                            (view, noViewFoundException) -> {
                                View parent = (View) view.getParent();
                                assertTrue(parent instanceof TabGroupColorPickerContainer);
                                TabGroupColorPickerContainer container =
                                        (TabGroupColorPickerContainer) parent;
                                float firstY = container.getChildAt(0).getY();
                                assertTrue(
                                        "Focused view should be in second row (Y > firstY)",
                                        view.getY() > firstY);
                            });
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

        // Set up the test as usual and show the menu on the tab group now that it is in a new
        // window.
        prepareStandardState().openContextMenu();

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
        waitForView(withText(moveToAnotherWindow)).click();

        // Sub-menu should be scrollable and should not have excess space.
        onView(withId(R.id.tab_group_action_menu_list)).check(matches(isScrollContainerMatcher));
        onView(withId(R.id.tab_group_action_menu_list)).check(matches(layoutParamsMatcher));

        // Go back to the main menu.
        waitForView(withText(moveToAnotherWindow)).click();

        // Main menu should not be scrollable again.
        onView(withId(R.id.tab_group_action_menu_list))
                .check(matches(not(isScrollContainerMatcher)));
    }

    private StripGroupTitleFacility<CtaPageStation> prepareStandardState() {
        // mPage is initialized in setUp() or updated by the test.
        mPage = mPage.openNewTabFast().loadAboutBlank();
        mPage = mPage.openNewTabFast().loadAboutBlank();

        ChromeTabbedActivity activity = (ChromeTabbedActivity) mPage.getActivity();
        StripGroupTitleFacility<CtaPageStation> groupTitle =
                createTabGroup(activity, /* isIncognito= */ false);

        // Re-initialize helper and manager for the current activity.
        mStripLayoutHelper = TabStripTestUtils.getActiveStripLayoutHelper(activity);
        mModalDialogManager = activity.getModalDialogManager();
        return groupTitle;
    }

    private StripGroupTitleFacility<CtaPageStation> prepareIncognitoState() {
        // Transition to incognito from the current regular page.
        mPage = mPage.openNewIncognitoTabOrWindowFast();
        mPage = mPage.openNewIncognitoTabFast();
        mPage = mPage.openNewIncognitoTabFast();

        ChromeTabbedActivity activity = (ChromeTabbedActivity) mPage.getActivity();
        // If we opened a new window, ensure the rule tracks it for cleanup.
        mActivityTestRule.getActivityTestRule().setActivity(activity);

        StripGroupTitleFacility<CtaPageStation> groupTitle =
                createTabGroup(activity, /* isIncognito= */ true);

        mStripLayoutHelper = TabStripTestUtils.getActiveStripLayoutHelper(activity);
        mModalDialogManager = activity.getModalDialogManager();
        return groupTitle;
    }

    private StripGroupTitleFacility<CtaPageStation> createTabGroup(
            ChromeTabbedActivity activity, boolean isIncognito) {
        TabStripFacility<CtaPageStation> tabStrip = mPage.expectTabStrip();
        TabGroupCreatedCondition groupCreated = new TabGroupCreatedCondition(mPage.tabModelElement);
        mPage.runTo(
                        () ->
                                TabStripTestUtils.createTabGroup(
                                        activity,
                                        isIncognito,
                                        /* firstIndex= */ 0,
                                        /* secondIndex= */ 1))
                .waitForAnd(groupCreated)
                .reach(tabStrip.settled());
        mTabGroupId = TabGroupId.toOptionalToken(groupCreated.getFromPast());
        return tabStrip.expectGroupTitle(groupCreated.getFromPast());
    }

    private void confirmActionDialog(
            TripBuilder openDialogTrigger, @StringRes int buttonTextRes, TabModel tabModel) {
        // ActionConfirmationDialog sets CUSTOM_BUTTON_BAR_VIEW with DualControlLayout
        // (R.id.button_primary), leaving ModalDialogView's standard R.id.positive_button GONE.
        ModalDialogFacility dialog = new ModalDialogFacility();
        ViewElement<Button> confirmButton =
                dialog.declareView(
                        dialog.dialogElement.descendant(
                                Button.class,
                                allOf(withId(R.id.button_primary), withText(buttonTextRes))));
        openDialogTrigger.enterFacility(dialog);
        confirmButton.clickTo().exitFacilityAnd().waitFor(tabGroupRemoved(tabModel));
    }

    private Condition tabGroupRemoved(TabModel tabModel) {
        return uiThreadCondition(
                "Tab group removed",
                () ->
                        whether(
                                !tabModel.containsTabGroup(
                                        TabGroupId.fromOptionalToken(mTabGroupId))));
    }

    private static EditText focusTitleEditText(
            TabStripGroupContextMenuFacility<CtaPageStation> menu) {
        TripBuilder clickTrigger = menu.titleElement.clickTo();
        EditText titleEditText = menu.titleElement.value();
        clickTrigger.waitFor(
                uiThreadCondition(
                        "Title edit text focused", () -> whether(titleEditText.isFocused())));
        return titleEditText;
    }

    private void updateGroupTitle(
            StripGroupTitleFacility<CtaPageStation> groupTitle,
            TabStripGroupContextMenuFacility<CtaPageStation> menu,
            String title) {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        KeyboardVisibilityDelegate delegate = activity.getWindowAndroid().getKeyboardDelegate();
        View compositorView = activity.getCompositorViewHolderForTesting();

        // Click group title text box to focus it for editing.
        focusTitleEditText(menu);

        // Enter new title in text box and press "enter" to dismiss menu and update group title.
        menu.titleElement.performViewAction(replaceText(title));

        // Verify keyboard and context menu are dismissed.
        menu.titleElement
                .performViewActionTo(pressImeActionButton())
                .exitFacilityAnd()
                .waitForAnd(
                        uiThreadCondition(
                                "Window focused and keyboard hidden",
                                () ->
                                        whether(
                                                activity.hasWindowFocus()
                                                        && !delegate.isKeyboardObscuring(
                                                                compositorView))))
                .reach(groupTitle.getTabStrip().settled());
    }
}
