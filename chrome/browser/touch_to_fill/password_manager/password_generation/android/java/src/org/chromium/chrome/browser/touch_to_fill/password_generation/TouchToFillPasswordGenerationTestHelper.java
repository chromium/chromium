// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.widget.TextView;

import java.util.concurrent.atomic.AtomicReference;

public class TouchToFillPasswordGenerationTestHelper {
    public static String acceptPasswordInGenerationBottomSheet() {
        String password = getTextFromTextView(R.id.password);
        onView(withId(R.id.use_password_button)).perform(click());
        return password;
    }

    public static void rejectPasswordInGenerationBottomSheet() {
        onView(withId(R.id.reject_password_button)).perform(click());
    }

    private static String getTextFromTextView(int id) {
        AtomicReference<String> textRef = new AtomicReference<>();
        onView(withId(id))
                .check((view, error) -> textRef.set(((TextView) view).getText().toString()));
        return textRef.get();
    }
}
