// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.chromium.chrome.browser.query_tiles.TileMatchers.withTile;
import static org.chromium.chrome.browser.query_tiles.ViewActions.scrollTo;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.matcher.BoundedMatcher;

import org.hamcrest.Description;
import org.hamcrest.Matcher;

import org.chromium.components.browser_ui.widget.image_tiles.ImageTile;

import java.util.List;

/** Helper {@link Matcher}s to validate the Query Tiles List UI. */
class ListMatchers {
    /**
     * Validates that a particular Android {@link View]} properly represents a list of {@link
     * ImageTile}s.
     * @param listMatcher The matcher to find the Android {@link View} representing the tile list.
     * @param expected    The expected list of {@link ImageTile}s.
     */
    public static <T extends ImageTile> void matchList(
            Matcher<View> listMatcher, List<T> expected) {
        onView(listMatcher).check(matches(isDisplayed()));
        onView(listMatcher).check(matches(withItemCount(expected.size())));
        for (int i = 0; i < expected.size(); i++) {
            onView(listMatcher)
                    .perform(scrollTo(i))
                    .check(matches(withElementAt(withTile(expected.get(i)), i)));
        }
    }

    /**
     * @param count The expected number of items in the {@link RecyclerView}.
     * @return      The {@link Matcher} instance to validate the right size of the {@link
     *         RecyclerView}.
     */
    public static Matcher<View> withItemCount(int count) {
        return new BoundedMatcher<View, RecyclerView>(RecyclerView.class) {
            // BoundedMatcher implementation.
            @Override
            public void describeTo(Description description) {
                description.appendText("with item count=" + count);
            }

            @Override
            public boolean matchesSafely(RecyclerView view) {
                return count == view.getAdapter().getItemCount();
            }
        };
    }

    /**
     * @return The {@link Matcher} instance to validate that the {@link RecyclerView} is scrolled to
     *         the front.
     */
    public static Matcher<View> isScrolledToFront() {
        return new BoundedMatcher<View, RecyclerView>(RecyclerView.class) {
            // BoundedMatcher implementation.
            @Override
            public void describeTo(Description description) {
                description.appendText("scrolled to front");
            }

            @Override
            protected boolean matchesSafely(RecyclerView recyclerView) {
                return recyclerView.computeHorizontalScrollOffset() == 0;
            }
        };
    }

    private static Matcher<View> withElementAt(Matcher<View> element, int position) {
        return new BoundedMatcher<View, RecyclerView>(RecyclerView.class) {
            @Override
            protected boolean matchesSafely(RecyclerView view) {
                RecyclerView.ViewHolder holder = view.findViewHolderForAdapterPosition(position);
                if (holder == null) return false;
                return element.matches(holder.itemView);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("with item at=" + position);
            }
        };
    }
}