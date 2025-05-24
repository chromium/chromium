// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test.util;

import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;

import android.view.View;
import android.webkit.ValueCallback;
import android.webkit.WebView;

import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;

import org.hamcrest.Matcher;

/** A {@link ViewAction} that executes JavaScript code in a {@link WebView}. */
public class JavaScriptExecutionViewAction implements ViewAction {
    // Taken from
    // javatests/com/google/android/apps/common
    // /testing/testrunner/web/JavaScriptIntegrationTest.java
    String mScript;
    public MyCallback callback;

    public static class MyCallback implements ValueCallback<String> {
        public String returnValue;

        @Override
        public void onReceiveValue(String returnValue) {
            this.returnValue = returnValue;
        }

        public MyCallback() {
            this.returnValue = null;
        }
    }

    public JavaScriptExecutionViewAction(String script) {
        this.mScript = script;
        this.callback = new MyCallback();
    }

    @Override
    public Matcher<View> getConstraints() {
        return isAssignableFrom(WebView.class);
    }

    @Override
    public String getDescription() {
        return "Execute JavaScript inside WebView.";
    }

    @Override
    public void perform(UiController controller, View view) {
        ((WebView) view).evaluateJavascript("javascript:" + mScript, callback);
    }

    /**
     * Returns the {@link ViewAction} to execute the script.
     *
     * @param script JavaScript code.
     * @return {@link ViewAction}.
     */
    public static ViewAction executeJs(String script) {
        return new JavaScriptExecutionViewAction(script);
    }
}
