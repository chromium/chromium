// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;
import android.os.IBinder;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.inspector.WindowInspector;

import androidx.test.espresso.Root;
import androidx.test.espresso.matcher.RootMatchers;

import com.google.common.collect.Lists;

import org.hamcrest.Matcher;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

@NullMarked
/** Searches Android Windows for specific Views and root Views. */
class ViewFinder {
    /**
     * Searches all Windows for root Views.
     *
     * @param activity If not null, restricts the search to that Activity's subwindows and dialogs.
     *     If null, searches all focusable windows and dialogs.
     * @return List of Roots found.
     */
    static List<Root> findRoots(@Nullable Activity activity) {
        ThreadUtils.assertOnUiThread();

        List<Root> matches = new ArrayList<>();

        IBinder activityToken = null;
        if (activity != null) {
            activityToken = activity.getWindow().getDecorView().getWindowToken();
        }
        List<View> globalWindowViews = WindowInspector.getGlobalWindowViews();
        for (View view : Lists.reverse(globalWindowViews)) {
            if (!(view.getLayoutParams()
                    instanceof WindowManager.LayoutParams windowLayoutParams)) {
                continue;
            }
            if ((windowLayoutParams.flags & WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE) != 0) {
                continue;
            }

            Root root =
                    new Root.Builder()
                            .withDecorView(view)
                            .withWindowLayoutParams(windowLayoutParams)
                            .build();

            // Include both subwindows of the activity and focused dialogs
            if ((activityToken == null || view.getApplicationWindowToken() == activityToken)
                    || (RootMatchers.isDialog().matches(root) && view.hasWindowFocus())) {
                matches.add(root);
            }
        }
        return matches;
    }

    /**
     * Searches multiple Roots for Views matching a |matcher|.
     *
     * @param roots List of Roots to search.
     * @param matcher Matcher to filter Views.
     * @return List of Views found and their respective Roots.
     */
    static List<ViewAndRoot> findViews(List<Root> roots, Matcher<View> matcher) {
        ThreadUtils.assertOnUiThread();

        List<ViewAndRoot> matches = new ArrayList<>();
        for (Root root : roots) {
            findViewsRecursive(root, root.getDecorView(), matcher, matches);
        }
        return matches;
    }

    private static void findViewsRecursive(
            Root root, View view, Matcher<View> matcher, List<ViewAndRoot> matches) {
        ThreadUtils.assertOnUiThread();

        if (matcher.matches(view)) {
            matches.add(new ViewAndRoot(view, root));
        }

        if (view instanceof ViewGroup viewGroup) {
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                findViewsRecursive(root, viewGroup.getChildAt(i), matcher, matches);
            }
        }
    }
}
