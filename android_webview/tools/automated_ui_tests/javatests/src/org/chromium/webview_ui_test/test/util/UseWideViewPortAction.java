// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test;

import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;

import android.view.View;
import android.webkit.WebView;

import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;

import org.hamcrest.Matcher;

/**
 * A ViewAction to set WebView's useWideViewPort setting
 * TODO(aluo): This may belong in WebViewUiTestRule.java
 */
public class UseWideViewPortAction implements ViewAction {
    private boolean mUseWideViewPort;

    public UseWideViewPortAction() {
        this(true);
    }

    public UseWideViewPortAction(boolean useWideViewPort) {
        mUseWideViewPort = useWideViewPort;
    }

    @Override
    public Matcher<View> getConstraints() {
        return isAssignableFrom(WebView.class);
    }

    @Override
    public String getDescription() {
        return "use wide viewport: " + mUseWideViewPort;
    }

    /**
     * Performs setUseWideViewPort then waits for completion.
     *
     * @param uiController the controller to use to interact with the UI.
     * @param view         the view to act upon. never null.
     */
    @Override
    public void perform(UiController uiController, View view) {
        WebView webview = (WebView) view;
        webview.getSettings().setUseWideViewPort(mUseWideViewPort);
        uiController.loopMainThreadUntilIdle();
    }
}
