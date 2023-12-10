// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.matcher.BoundedMatcher;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

/** Contains useful RecyclerViewMatcher. */
public class RecyclerViewMatcherUtils {
    /**
     * This view matcher matches a RecyclerView that has a given number of items in its adapter.
     *
     * @param itemCount The matches item count.
     * @return A matcher that matches RecyclerView with its adapter item count.
     */
    public static Matcher<View> adapterHasItemCount(int itemCount) {
        return new BoundedMatcher<View, RecyclerView>(RecyclerView.class) {
            @Override
            protected boolean matchesSafely(RecyclerView recyclerView) {
                return recyclerView.getAdapter().getItemCount() == itemCount;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("RecyclerView.Adapter has " + itemCount + " items");
            }
        };
    }

    /**
     * This view matcher matches a RecyclerView that has a view that matches the given view matcher
     * at the given adapter position.
     *
     * First this matcher scrolls the RecyclerView to the given position and then matches with the
     * given view matcher.
     *
     * @param position The matches adapter position.
     * @param itemMatcher A view matcher to match.
     * @return A matcher that matches RecyclerView with its adapter item position and the given view
     *         matcher.
     */
    public static Matcher<View> atPosition(int position, Matcher<View> itemMatcher) {
        return new BoundedMatcher<View, RecyclerView>(RecyclerView.class) {
            @Override
            protected boolean matchesSafely(RecyclerView recyclerView) {
                recyclerView.scrollToPosition(position);
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(position);

                if (viewHolder == null) return false;

                return itemMatcher.matches(viewHolder.itemView);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("has view " + itemMatcher + " at position " + position);
            }
        };
    }

    /**
     * This matcher matches RecyclerView that has a ViewHolder that matches the given view holder
     * matcher at the given adapter position.
     *
     * @param position The adapter position.
     * @param viewHolderMatcher A view holder to match.
     * @return A matcher that matches view at adapter position and matches the given viewHolder
     *         matcher.
     */
    public static Matcher<View> atPositionWithViewHolder(
            int position, Matcher<RecyclerView.ViewHolder> viewHolderMatcher) {
        return new BoundedMatcher<View, RecyclerView>(RecyclerView.class) {
            @Override
            protected boolean matchesSafely(RecyclerView recyclerView) {
                recyclerView.scrollToPosition(position);
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(position);

                if (viewHolder == null) return false;

                return viewHolderMatcher.matches(viewHolder);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText(viewHolderMatcher + " at position " + position);
            }
        };
    }

    /**
     * This matcher matches the ViewHolder that has an adapter position equals to the given
     * position.
     *
     * @param position The position to match.
     * @return A matcher that matches the viewHolder at the given position.
     */
    public static Matcher<RecyclerView.ViewHolder> withViewHolderAtPosition(int position) {
        return new TypeSafeMatcher<RecyclerView.ViewHolder>() {
            @Override
            protected boolean matchesSafely(RecyclerView.ViewHolder viewHolder) {
                return viewHolder.getAdapterPosition() == position;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("ViewHolder with adapter position: " + position);
            }
        };
    }

    /**
     * This matcher matches a ViewHolder that has the given item type.
     *
     * @param itemType The item type to match.
     * @return A matcher that matches a ViewHolder with the given item type.
     */
    public static Matcher<RecyclerView.ViewHolder> withItemType(int itemType) {
        return new TypeSafeMatcher<RecyclerView.ViewHolder>() {
            @Override
            protected boolean matchesSafely(RecyclerView.ViewHolder viewHolder) {
                return viewHolder.getItemViewType() == itemType;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("ViewHolder with item type: " + itemType);
            }
        };
    }
}
