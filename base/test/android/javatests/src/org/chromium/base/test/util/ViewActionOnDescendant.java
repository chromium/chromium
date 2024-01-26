// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import static androidx.test.espresso.Espresso.onView;

import android.view.View;

import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.util.TreeIterables;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import java.util.ArrayList;
import java.util.List;

/**
 * ViewAction that performs another ViewAction on a descendant.
 *
 * <p>Example usage:
 *
 * <pre>
 *     ViewActionOnDescendant.performOnRecyclerViewNthItemDescendant(
 *             withId(R.id.my_recycler_view), index, withId(R.id.small_button), click()))
 * </pre>
 *
 * Can be used either through the convenience static methods or instantiated directly.
 */
public class ViewActionOnDescendant implements ViewAction {

    private final Matcher<View> mDescendantMatcher;
    private final ViewAction mViewAction;

    /**
     * @param descendantMatcher View matcher that should match a single descendant within a
     *     RecyclerView item
     * @param viewAction ViewAction to perform on that descendant
     */
    public ViewActionOnDescendant(Matcher<View> descendantMatcher, ViewAction viewAction) {
        mDescendantMatcher = descendantMatcher;
        mViewAction = viewAction;
    }

    /**
     * Performs a ViewAction on a single descendant of the nth item of a RecyclerView.
     *
     * @param recyclerViewMatcher View matcher to find the RecyclerView
     * @param index position of the item in the RecyclerView to search for a descendant
     * @param descendantMatcher View matcher that should match a single descendant within a
     *     RecyclerView item
     * @param viewAction ViewAction to perform on that descendant
     */
    public static void performOnRecyclerViewNthItemDescendant(
            Matcher<View> recyclerViewMatcher,
            int index,
            Matcher<View> descendantMatcher,
            ViewAction viewAction) {
        performOnRecyclerViewNthItem(
                recyclerViewMatcher,
                index,
                new ViewActionOnDescendant(descendantMatcher, viewAction));
    }

    /**
     * Performs a ViewAction on the nth item of a RecyclerView.
     *
     * @param recyclerViewMatcher View matcher to find the RecyclerView
     * @param index position of the item in the RecyclerView to perform the action on
     * @param viewAction ViewAction to perform on that item
     */
    public static void performOnRecyclerViewNthItem(
            Matcher<View> recyclerViewMatcher, int index, ViewAction viewAction) {
        onView(recyclerViewMatcher).perform(RecyclerViewActions.scrollToPosition(index));
        onView(recyclerViewMatcher)
                .perform(RecyclerViewActions.actionOnItemAtPosition(index, viewAction));
    }

    @Override
    public String getDescription() {
        return String.format(
                "perform %s on a descendant %s",
                mViewAction.getDescription(), StringDescription.asString(mDescendantMatcher));
    }

    @Override
    public Matcher<View> getConstraints() {
        return null;
    }

    @Override
    public void perform(UiController uiController, View view) {
        List<View> matches = new ArrayList<>();
        for (View v : TreeIterables.breadthFirstViewTraversal(view)) {
            if (mDescendantMatcher.matches(v)) {
                matches.add(v);
            }
        }
        if (matches.size() == 0) {
            throw new RuntimeException(
                    String.format("No views %s", StringDescription.asString(mDescendantMatcher)));
        } else if (matches.size() > 1) {
            throw new RuntimeException(
                    String.format(
                            "Multiple views %s", StringDescription.asString(mDescendantMatcher)));
        }

        mViewAction.perform(uiController, matches.get(0));
    }
}
