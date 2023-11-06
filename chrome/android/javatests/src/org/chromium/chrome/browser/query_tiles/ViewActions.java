// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.matcher.ViewMatchers;

import org.hamcrest.Matcher;
import org.hamcrest.core.AllOf;

/** Helper {@link ViewAction}s to drive the Query Tiles UI. */
final class ViewActions {
    private ViewActions() {}

    /**
     * Creates a {@link ViewAction} that will scroll to a certain element in the list.
     *
     * @param position The position in the {@link RecyclerView} to scroll to.
     * @return A {@link ViewAction} that will scroll a {@link RecyclerView} to {@code position}.
     */
    public static ViewAction scrollTo(int position) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return AllOf.allOf(
                        ViewMatchers.isAssignableFrom(RecyclerView.class),
                        ViewMatchers.isDisplayed());
            }

            @Override
            public String getDescription() {
                return "scroll RecyclerView to " + position;
            }

            @Override
            public void perform(UiController uiController, View view) {
                ((RecyclerView) view).scrollToPosition(position);
                uiController.loopMainThreadUntilIdle();
            }
        };
    }
}
