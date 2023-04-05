// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import android.text.TextUtils;
import android.view.View;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.test.espresso.matcher.BoundedMatcher;

import org.hamcrest.Description;
import org.hamcrest.Matcher;

import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTile;
import org.chromium.components.query_tiles.QueryTile;

/** Helper {@link Matcher}s to validate the Query Tiles UI. */
final class TileMatchers {
    private TileMatchers() {}

    /**
     * Builds a {@link Matcher} that finds a {@link View} that corresponds to a particular {@link
     * ImageTile}.
     * @param tile The {@link ImageTile} to match.
     * @return     The {@link Matcher} instance.
     */
    public static Matcher<View> withTile(ImageTile tile) {
        return new BoundedMatcher<View, RelativeLayout>(RelativeLayout.class) {
            // BoundedMatcher implementation.
            @Override
            public void describeTo(Description description) {
                description.appendText("with tile=" + tile.id);
            }

            @Override
            public boolean matchesSafely(RelativeLayout view) {
                return TextUtils.equals(
                        ((TextView) view.findViewById(R.id.title)).getText(), tile.displayTitle);
            }
        };
    }

    /**
     * Builds a {@link Matcher} that finds a {@link View} that corresponds to a particular {@link
     * ImageTile} chip.
     * @param tile The {@link ImageTile} to match.
     * @return     The {@link Matcher} instance.
     */
    public static Matcher<View> withChip(QueryTile tile) {
        return new BoundedMatcher<View, ChipView>(ChipView.class) {
            // BoundedMatcher implementation.
            @Override
            public void describeTo(Description description) {
                description.appendText("with chip=" + tile.id);
            }

            @Override
            public boolean matchesSafely(ChipView view) {
                return TextUtils.equals(view.getPrimaryTextView().getText(), tile.queryText);
            }
        };
    }
}