// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test.util;

import static android.support.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static android.support.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static android.support.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;

import static org.hamcrest.Matchers.allOf;

import android.support.test.espresso.UiController;
import android.support.test.espresso.ViewAction;
import android.support.test.espresso.matcher.ViewMatchers;
import android.view.View;
import android.webkit.WebView;

import androidx.annotation.NonNull;

import org.hamcrest.Matcher;

/**
 * Actions to help with WebView tests
 */
public class Actions {

    public static ViewAction setUseWideViewPort() {
        return setUseWideViewPort(true);
    }
    public static ViewAction setUseWideViewPort(final boolean useWideViewPort) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return isAssignableFrom(WebView.class);
            }

            @Override
            public String getDescription() {
                return "use wide viewport: " + useWideViewPort;
            }

            /**
             * Performs setUseWideViewPort then waits for completion.
             *
             * @param uiController the controller to use to interact with the UI.
             * @param view         the view to act upon.  Must be webview or a subclass thereof.
             */
            @Override
            public void perform(UiController uiController, @NonNull View view) {
                if (!(view instanceof WebView)) return;
                WebView webview = (WebView) view;
                webview.getSettings().setUseWideViewPort(useWideViewPort);
                uiController.loopMainThreadUntilIdle();
            }
        };
    }
    public static ViewAction scrollBy(final int x, final int y) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return allOf(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE),
                    isDescendantOfA(isAssignableFrom(View.class)));
            }

            @Override
            public String getDescription() {
                return "scroll by: " + x + "," + y;
            }

            /**
             * Performs setUseWideViewPort then waits for completion.
             *
             * @param uiController the controller to use to interact with the UI.
             * @param view         the view to act upon.
             */
            @Override
            public void perform(UiController uiController, @NonNull View view) {
                view.scrollBy(x, y);
                uiController.loopMainThreadUntilIdle();
            }
        };
    }
}
