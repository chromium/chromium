// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static org.hamcrest.Matchers.is;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.view.View;
import android.widget.ListView;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.base.ThreadUtils;

import java.util.concurrent.ExecutionException;

/** Util methods for developer UI tests. */
public class DeveloperUiTestUtils {
    /**
     * Matches that a {@link ListView} has a specific number of items.
     *
     * @param intMatcher {@line Matcher} class that matches a given integer.
     */
    public static Matcher<View> withCount(final Matcher<Integer> intMatcher) {
        return new TypeSafeMatcher<>() {
            @Override
            public boolean matchesSafely(View view) {
                if (!(view instanceof ListView)) {
                    return false;
                }
                int count = ((ListView) view).getCount();
                return intMatcher.matches(count);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("with child-count: ");
                intMatcher.describeTo(description);
            }
        };
    }

    /** Matches that a {@link ListView} has a specific number of items */
    public static Matcher<View> withCount(final int itemCount) {
        return withCount(is(itemCount));
    }

    public static String getClipBoardTextOnUiThread(Context context) throws ExecutionException {
        // ClipManager service has to be called on the UI main thread.
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ClipboardManager clipboardManager =
                            (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
                    return clipboardManager.getPrimaryClip().getItemAt(0).getText().toString();
                });
    }

    public static void setClipBoardTextOnUiThread(Context context, String key, String value)
            throws ExecutionException {
        // ClipManager service has to be called on the UI main thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ClipboardManager clipboardManager =
                            (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
                    ClipData clip = ClipData.newPlainText(key, value);
                    clipboardManager.setPrimaryClip(clip);
                });
    }

    // Don't instantiate this class.
    private DeveloperUiTestUtils() {}
}
