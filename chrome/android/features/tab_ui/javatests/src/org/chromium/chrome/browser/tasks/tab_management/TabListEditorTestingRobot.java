// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollToPosition;
import static androidx.test.espresso.matcher.ViewMatchers.isClickable;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isFocusable;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewMatcherUtils.atPosition;
import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewMatcherUtils.atPositionWithViewHolder;
import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewMatcherUtils.withItemType;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.IdRes;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.NoMatchingRootException;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.matcher.BoundedMatcher;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Assert;

import org.chromium.base.test.util.RawFailureHandler;
import org.chromium.base.test.util.ViewActionOnDescendant;
import org.chromium.chrome.test.R;

/**
 * This is the testing util class for TabListEditor. It's used to perform action and verify result
 * within the TabListEditor.
 */
public class TabListEditorTestingRobot {
    /**
     * @param viewMatcher A matcher that matches a view.
     * @return A matcher that matches view in the {@link TabListEditorLayout} based on the given
     *     matcher.
     */
    public static Matcher<View> inTabListEditor(Matcher<View> viewMatcher) {
        return allOf(isDescendantOfA(instanceOf(TabListEditorLayout.class)), viewMatcher);
    }

    /**
     * @return A view matcher that matches the item is selected.
     */
    public static Matcher<View> itemIsSelected() {
        return new BoundedMatcher<View, TabGridView>(TabGridView.class) {
            private TabGridView mSelectableTabGridView;

            @Override
            protected boolean matchesSafely(TabGridView selectableTabGridView) {
                mSelectableTabGridView = selectableTabGridView;

                return mSelectableTabGridView.isChecked()
                        && actionButtonSelected()
                        && TabUiTestHelper.isTabViewSelected(mSelectableTabGridView);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("Item is selected");
            }

            private boolean actionButtonSelected() {
                return mSelectableTabGridView
                                .getResources()
                                .getInteger(R.integer.list_item_level_selected)
                        == mSelectableTabGridView
                                .findViewById(R.id.action_button)
                                .getBackground()
                                .getLevel();
            }
        };
    }

    /**
     * @return A view matcher that matches a divider view.
     */
    public static Matcher<View> isDivider() {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View view) {
                return view.getId() == R.id.divider_view;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("is divider");
            }
        };
    }

    public final TabListEditorTestingRobot.Result resultRobot;
    public final TabListEditorTestingRobot.Action actionRobot;

    public TabListEditorTestingRobot() {
        resultRobot = new Result();
        actionRobot = new Action();
    }

    /** This Robot is used to perform action within the TabListEditor. */
    public static class Action {
        public TabListEditorTestingRobot.Action clickItemAtAdapterPosition(int position) {
            onView(inTabListEditor(withId(R.id.tab_list_recycler_view)))
                    .perform(actionOnItemAtPosition(position, click()));
            return this;
        }

        public TabListEditorTestingRobot.Action clickActionButtonAdapterPosition(
                int position, @IdRes int actionButtonId) {
            ViewActionOnDescendant.performOnRecyclerViewNthItemDescendant(
                    inTabListEditor(withId(R.id.tab_list_recycler_view)),
                    position,
                    withId(actionButtonId),
                    click());
            return this;
        }

        public TabListEditorTestingRobot.Action clickToolbarMenuButton() {
            onView(
                            inTabListEditor(
                                    allOf(
                                            withId(R.id.list_menu_button),
                                            withParent(withId(R.id.action_view_layout)))))
                    .perform(click());
            return this;
        }

        public TabListEditorTestingRobot.Action clickToolbarActionView(int id) {
            onView(inTabListEditor(withId(id))).perform(click());
            return this;
        }

        public TabListEditorTestingRobot.Action clickToolbarMenuItem(String text) {
            onView(withText(text)).perform(click());
            return this;
        }

        public TabListEditorTestingRobot.Action clickToolbarNavigationButton() {
            clickToolbarNavigationButton(R.string.accessibility_tab_selection_editor_back_button);
            return this;
        }

        public TabListEditorTestingRobot.Action clickToolbarNavigationButton(
                @IdRes int navigationButtonIdRes) {
            onView(
                            inTabListEditor(
                                    allOf(
                                            withContentDescription(navigationButtonIdRes),
                                            withParent(withId(R.id.action_bar)))))
                    .perform(click());
            return this;
        }

        public TabListEditorTestingRobot.Action clickEndButtonAtAdapterPosition(int position) {
            clickViewIdAtAdapterPosition(0, R.id.end_button);
            return this;
        }

        public TabListEditorTestingRobot.Action clickViewIdAtAdapterPosition(
                int position, @IdRes int id) {
            onView(inTabListEditor(withId(R.id.tab_list_recycler_view)))
                    .perform(
                            new ViewAction() {
                                @Override
                                public Matcher<View> getConstraints() {
                                    return isDisplayed();
                                }

                                @Override
                                public String getDescription() {
                                    return "click on end button of item with index "
                                            + String.valueOf(position);
                                }

                                @Override
                                public void perform(UiController uiController, View view) {
                                    RecyclerView recyclerView = (RecyclerView) view;
                                    RecyclerView.ViewHolder viewHolder =
                                            recyclerView.findViewHolderForAdapterPosition(position);
                                    if (viewHolder.itemView == null) return;
                                    viewHolder.itemView.findViewById(id).performClick();
                                }
                            });
            return this;
        }
    }

    /** This Robot is used to verify result within the TabListEditor. */
    public static class Result {
        public TabListEditorTestingRobot.Result verifyTabListEditorIsVisible() {
            onView(allOf(instanceOf(TabListEditorLayout.class), withId(R.id.selectable_list)))
                    .check(matches(isDisplayed()));
            return this;
        }

        public TabListEditorTestingRobot.Result verifyTabListEditorIsHidden() {
            try {
                onView(allOf(instanceOf(TabListEditorLayout.class), withId(R.id.selectable_list)))
                        // DefaultFailureHandler breaks when dumping the view hierarchy that
                        // contains a webview. See crbug.com/339675001.
                        .withFailureHandler(RawFailureHandler.getInstance())
                        .check(matches(isDisplayed()));
            } catch (NoMatchingRootException | NoMatchingViewException e) {
                return this;
            }

            assert false : "TabListEditor should be hidden, but it's not.";
            return this;
        }

        public TabListEditorTestingRobot.Result verifyToolbarSelectionTextWithResourceId(
                int resourceId) {
            onView(inTabListEditor(withText(resourceId))).check(matches(isDisplayed()));
            return this;
        }

        public TabListEditorTestingRobot.Result verifyToolbarSelectionText(String text) {
            // Text updates are animated. Wait for the right text if animations cannot be disabled.
            onViewWaiting(inTabListEditor(withText(text))).check(matches(isDisplayed()));
            return this;
        }

        public TabListEditorTestingRobot.Result verifyToolbarActionViewWithText(
                int id, String text) {
            onView(inTabListEditor(withId(id))).check(matches(withText(text)));
            return this;
        }

        public TabListEditorTestingRobot.Result verifyToolbarActionViewDisabled(int id) {
            onView(inTabListEditor(withId(id))).check(matches(not(isEnabled())));
            return this;
        }

        public TabListEditorTestingRobot.Result verifyToolbarActionViewEnabled(int id) {
            onView(inTabListEditor(withId(id))).check(matches(isEnabled()));
            return this;
        }

        public TabListEditorTestingRobot.Result verifyToolbarMenuItemState(
                String text, boolean enabled) {
            onView(withText(text)).check(matches(enabled ? isEnabled() : not(isEnabled())));
            return this;
        }

        public TabListEditorTestingRobot.Result verifyToolbarMenuItemWithContentDescription(
                String text, String contentDescription) {
            onView(allOf(withText(text), withContentDescription(contentDescription)))
                    .check(matches(isDisplayed()));
            return this;
        }

        public TabListEditorTestingRobot.Result verifyHasAtLeastNItemVisible(int count) {
            onView(inTabListEditor(withId(R.id.tab_list_recycler_view)))
                    .check(
                            (v, noMatchException) -> {
                                if (noMatchException != null) throw noMatchException;

                                Assert.assertTrue(v instanceof RecyclerView);
                                Assert.assertTrue(((RecyclerView) v).getChildCount() >= count);
                            });
            return this;
        }

        public TabListEditorTestingRobot.Result verifyAdapterHasItemCount(int count) {
            onView(inTabListEditor(withId(R.id.tab_list_recycler_view)))
                    .check(matches(RecyclerViewMatcherUtils.adapterHasItemCount(count)));
            return this;
        }

        public TabListEditorTestingRobot.Result verifyItemNotSelectedAtAdapterPosition(
                int position) {
            onView(inTabListEditor(withId(R.id.tab_list_recycler_view)))
                    .check(
                            matches(
                                    not(
                                            RecyclerViewMatcherUtils.atPosition(
                                                    position, itemIsSelected()))));
            return this;
        }

        public TabListEditorTestingRobot.Result verifyItemSelectedAtAdapterPosition(
                int position) {
            onView(inTabListEditor(withId(R.id.tab_list_recycler_view)))
                    .check(
                            matches(
                                    RecyclerViewMatcherUtils.atPosition(
                                            position, itemIsSelected())));
            return this;
        }

        public TabListEditorTestingRobot.Result verifyUndoSnackbarWithTextIsShown(
                String text) {
            onView(withText(text)).check(matches(isDisplayed()));
            return this;
        }

        public Result verifyDividerAlwaysStartsAtTheEdgeOfScreen() {
            onView(
                            inTabListEditor(
                                    allOf(
                                            isDivider(),
                                            withParent(withId(R.id.tab_list_recycler_view)))))
                    .check(matches(isDisplayed()))
                    .check(
                            (v, noMatchException) -> {
                                if (noMatchException != null) throw noMatchException;

                                View parentView = (View) v.getParent();
                                Assert.assertEquals(parentView.getPaddingStart(), (int) v.getX());
                            });
            return this;
        }

        public Result verifyDividerAlwaysStartsAtTheEdgeOfScreenAtPosition(int position) {
            onView(inTabListEditor(withId(R.id.tab_list_recycler_view)))
                    .perform(scrollToPosition(position));

            onView(inTabListEditor(atPosition(position, isDivider())))
                    .check(matches(isDisplayed()))
                    .check(
                            (v, noMatchException) -> {
                                if (noMatchException != null) throw noMatchException;

                                View parentView = (View) v.getParent();
                                Assert.assertEquals(parentView.getPaddingStart(), (int) v.getX());
                            });

            return this;
        }

        public Result verifyDividerNotClickableNotFocusable() {
            onView(
                            inTabListEditor(
                                    allOf(
                                            isDivider(),
                                            withParent(withId(R.id.tab_list_recycler_view)))))
                    .check(matches(not(isClickable())))
                    .check(matches(not(isFocusable())));
            return this;
        }

        /**
         * Verifies the TabListEditor has an ItemView at given position that matches the given
         * targetItemViewType.
         *
         * First this method scrolls to the given adapter position to make sure ViewHolder for the
         * given position is visible.
         *
         * @param position Adapter position.
         * @param targetItemViewType The item view type to be matched.
         * @return {@link Result} to do chain verification.
         */
        public Result verifyHasItemViewTypeAtAdapterPosition(int position, int targetItemViewType) {
            onView(inTabListEditor(withId(R.id.tab_list_recycler_view)))
                    .perform(scrollToPosition(position));
            onView(
                            inTabListEditor(
                                    atPositionWithViewHolder(
                                            position, withItemType(targetItemViewType))))
                    .check(matches(isDisplayed()));
            return this;
        }

        public Result verifyTabListEditorHasTopMargin(int topMargin) {
            onView(withId(R.id.selectable_list))
                    .check(
                            (v, noMatchException) -> {
                                if (noMatchException != null) throw noMatchException;
                                Assert.assertEquals(
                                        topMargin,
                                        ((MarginLayoutParams) v.getLayoutParams()).topMargin);
                            });
            return this;
        }
    }
}
