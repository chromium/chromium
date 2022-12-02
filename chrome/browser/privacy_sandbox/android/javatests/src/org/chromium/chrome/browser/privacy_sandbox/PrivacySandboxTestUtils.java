// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.hamcrest.Matcher;

/**
 * Test utilities for various privacy_sandbox tests.
 */
public final class PrivacySandboxTestUtils {
    /**
     * Click an ImageButton located next to a View that contains the given text.
     *
     * @param text The text contained in the View adjacent to the ImageButton.
     */
    public static void clickImageButtonNextToText(String text) {
        onView(allOf(withId(R.id.image_button), withParent(hasSibling(withChild(withText(text))))))
                .perform(click());
    }

    /**
     * Matcher for a {@link Topic} based only on the name.
     *
     * @param name The name of the {@link Topic}.
     * @return The Matcher for the {@link Topic}.
     */
    public static Matcher<Topic> withTopic(String name) {
        return new BaseMatcher<>() {
            @Override
            public boolean matches(Object o) {
                return ((Topic) o).getName().equals(name);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("Should contain " + name);
            }
        };
    }
}
