// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.inspector.WindowInspector;

import androidx.test.espresso.Root;
import androidx.test.espresso.matcher.RootMatchers;

import com.google.common.collect.Lists;

import org.hamcrest.Matcher;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.RootSpec.RootType;
import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.List;

/** Searches Android Windows for root Views and specific Views in those roots. */
@NullMarked
class InternalViewFinder {
    /**
     * Searches all Windows for root Views.
     *
     * @param rootSpec Which scope to restrict the search to.
     * @return List of Roots found.
     */
    static List<Root> findRoots(RootSpec rootSpec) {
        ThreadUtils.assertOnUiThread();

        List<Root> matches = new ArrayList<>();

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

            boolean rootMatches;
            if (rootSpec.getType() == RootType.ANY_ROOT) {
                rootMatches = true;
            } else if (RootMatchers.isDialog().matches(root) && view.hasWindowFocus()) {
                rootMatches = rootSpec.allowsFocusedDialogs();
            } else {
                // Subwindows of the activity.
                rootMatches = rootSpec.allowsWindowToken(view.getApplicationWindowToken());
            }

            if (rootMatches) {
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
