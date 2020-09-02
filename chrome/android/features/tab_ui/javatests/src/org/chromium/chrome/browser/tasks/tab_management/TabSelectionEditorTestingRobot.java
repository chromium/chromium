// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollToPosition;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isClickable;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isFocusable;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewMatcherUtils.atPosition;
import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewMatcherUtils.atPositionWithViewHolder;
import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewMatcherUtils.withItemType;

import android.os.Build;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.NoMatchingRootException;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.Root;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.matcher.BoundedMatcher;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Assert;

/**
 * This is the testing util class for TabSelectionEditor. It's used to perform action and verify
 * result within the TabSelectionEditor.
 */
public class TabSelectionEditorTestingRobot {
    /**
     * @return A root matcher that matches the TabSelectionEditor popup decor view.
     */
    public static Matcher<Root> isTabSelectionEditorPopup() {
        return new TypeSafeMatcher<Root>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("is TabSelectionEditor Popup");
            }

            @Override
            public boolean matchesSafely(Root root) {
                if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                    return withDecorView(
                            withClassName(is(TabSelectionEditorLayout.class.getName())))
                            .matches(root);
                } else {
                    return withDecorView(
                            withClassName(is("android.widget.PopupWindow$PopupDecorView")))
                            .matches(root);
                }
            }
        };
    }

    /**
     * @return A view matcher that matches the item is selected.
     */
    public static Matcher<View> itemIsSelected() {
        return new BoundedMatcher<View, SelectableTabGridView>(SelectableTabGridView.class) {
            private SelectableTabGridView mSelectableTabGridView;
            @Override
            protected boolean matchesSafely(SelectableTabGridView selectableTabGridView) {
                mSelectableTabGridView = selectableTabGridView;

                return mSelectableTabGridView.isChecked() && actionButtonSelected()
                        && highlightIndicatorIsVisible();
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("Item is selected");
            }

            private boolean actionButtonSelected() {
                return mSelectableTabGridView.getResources().getInteger(
                               org.chromium.chrome.tab_ui.R.integer.list_item_level_selected)
                        == mSelectableTabGridView
                                   .findViewById(org.chromium.chrome.tab_ui.R.id.action_button)
                                   .getBackground()
                                   .getLevel();
            }

            private boolean highlightIndicatorIsVisible() {
                if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                    return mSelectableTabGridView
                                   .findViewById(org.chromium.chrome.tab_ui.R.id
                                                         .selected_view_below_lollipop)
                                   .getVisibility()
                            == View.VISIBLE;
                } else {
                    return mSelectableTabGridView.getForeground() != null;
                }
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
                return view.getId() == org.chromium.chrome.tab_ui.R.id.divider_view;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("is divider");
            }
        };
    }

    public final TabSelectionEditorTestingRobot.Result resultRobot;
    public final TabSelectionEditorTestingRobot.Action actionRobot;

    public TabSelectionEditorTestingRobot() {
        resultRobot = new Result();
        actionRobot = new Action();
    }

    /**
     * This Robot is used to perform action within the TabSelectionEditor.
     */
    public static class Action {
        public TabSelectionEditorTestingRobot.Action clickItemAtAdapterPosition(int position) {
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .perform(actionOnItemAtPosition(position, click()));
            return this;
        }

        public TabSelectionEditorTestingRobot.Action clickToolbarActionButton() {
            onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.action_button),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.action_bar))))
                    .inRoot(isTabSelectionEditorPopup())
                    .perform(click());
            return this;
        }

        public TabSelectionEditorTestingRobot.Action clickToolbarNavigationButton() {
            onView(allOf(withContentDescription(org.chromium.chrome.tab_ui.R.string.close),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.action_bar))))
                    .inRoot(isTabSelectionEditorPopup())
                    .perform(click());
            return this;
        }

        public TabSelectionEditorTestingRobot.Action clickEndButtonAtAdapterPosition(int position) {
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .perform(new ViewAction() {
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
                            viewHolder.itemView
                                    .findViewById(org.chromium.chrome.tab_ui.R.id.end_button)
                                    .performClick();
                        }
                    });
            return this;
        }
    }

    /**
     * This Robot is used to verify result within the TabSelectionEditor.
     */
    public static class Result {
        public TabSelectionEditorTestingRobot.Result verifyTabSelectionEditorIsVisible() {
            onView(withId(org.chromium.chrome.tab_ui.R.id.selectable_list))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(isDisplayed()));
            return this;
        }

        public TabSelectionEditorTestingRobot.Result verifyTabSelectionEditorIsHidden() {
            try {
                onView(withId(org.chromium.chrome.tab_ui.R.id.selectable_list))
                        .inRoot(isTabSelectionEditorPopup())
                        .check(matches(isDisplayed()));
            } catch (NoMatchingRootException | NoMatchingViewException e) {
                return this;
            }

            assert false : "TabSelectionEditor should be hidden, but it's not.";
            return this;
        }

        public TabSelectionEditorTestingRobot.Result verifyToolbarSelectionTextWithResourceId(
                int resourceId) {
            onView(withText(resourceId))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(isDisplayed()));
            return this;
        }

        public TabSelectionEditorTestingRobot.Result verifyToolbarSelectionText(String text) {
            onView(withText(text))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(isDisplayed()));
            return this;
        }

        public TabSelectionEditorTestingRobot.Result verifyToolbarActionButtonWithResourceId(
                int resourceId) {
            onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.action_button),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.action_bar))))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(withText(resourceId)));
            return this;
        }

        public TabSelectionEditorTestingRobot.Result verifyToolbarActionButtonWithText(
                String text) {
            onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.action_button),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.action_bar))))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(withText(text)));
            return this;
        }

        public TabSelectionEditorTestingRobot.Result verifyToolbarActionButtonDisabled() {
            onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.action_button),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.action_bar))))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(not(isEnabled())));
            return this;
        }

        public TabSelectionEditorTestingRobot.Result verifyToolbarActionButtonEnabled() {
            onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.action_button),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.action_bar))))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(isEnabled()));
            return this;
        }

        public TabSelectionEditorTestingRobot.Result verifyHasAtLeastNItemVisible(int count) {
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .check((v, noMatchException) -> {
                        if (noMatchException != null) throw noMatchException;

                        Assert.assertTrue(v instanceof RecyclerView);
                        Assert.assertTrue(((RecyclerView) v).getChildCount() >= count);
                    });
            return this;
        }

        public TabSelectionEditorTestingRobot.Result verifyAdapterHasItemCount(int count) {
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(RecyclerViewMatcherUtils.adapterHasItemCount(count)));
            return this;
        }

        public TabSelectionEditorTestingRobot.Result verifyItemNotSelectedAtAdapterPosition(
                int position) {
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(
                            not(RecyclerViewMatcherUtils.atPosition(position, itemIsSelected()))));
            return this;
        }

        public TabSelectionEditorTestingRobot.Result verifyItemSelectedAtAdapterPosition(
                int position) {
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(
                            RecyclerViewMatcherUtils.atPosition(position, itemIsSelected())));
            return this;
        }

        public TabSelectionEditorTestingRobot.Result verifyUndoSnackbarWithTextIsShown(
                String text) {
            onView(withText(text)).check(matches(isDisplayed()));
            return this;
        }

        public Result verifyDividerAlwaysStartsAtTheEdgeOfScreen() {
            onView(allOf(isDivider(),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(isDisplayed()))
                    .check((v, noMatchException) -> {
                        if (noMatchException != null) throw noMatchException;

                        View parentView = (View) v.getParent();
                        Assert.assertEquals(parentView.getPaddingStart(), (int) v.getX());
                    });
            return this;
        }

        public Result verifyDividerAlwaysStartsAtTheEdgeOfScreenAtPosition(int position) {
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .perform(scrollToPosition(position));

            onView(atPosition(position, isDivider()))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(isDisplayed()))
                    .check((v, noMatchException) -> {
                        if (noMatchException != null) throw noMatchException;

                        View parentView = (View) v.getParent();
                        Assert.assertEquals(parentView.getPaddingStart(), (int) v.getX());
                    });

            return this;
        }

        public Result verifyDividerNotClickableNotFocusable() {
            onView(allOf(isDivider(),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(not(isClickable())))
                    .check(matches(not(isFocusable())));
            return this;
        }

        /**
         * Verifies the TabSelectionEditor has an ItemView at given position that matches the given
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
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .perform(scrollToPosition(position));
            onView(atPositionWithViewHolder(position, withItemType(targetItemViewType)))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(isDisplayed()));
            return this;
        }
    }
}
