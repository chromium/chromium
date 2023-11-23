// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.webview_ui_test.test.util;

/*
 * Creates an action which executes on webview.
 */
public abstract class Action {
    /// Executes the action on the WebView.
    public abstract boolean execute(PerformActions performActions) throws Throwable;

    // Creates an action that loads the desired url in the webview upon execution.
    private static class StartingPageAction extends Action {
        private String mUrl;

        public StartingPageAction(String url) {
            mUrl = url;
        }

        @Override
        public boolean execute(PerformActions performActions) throws Throwable {
            performActions.loadUrl(mUrl);
            return true;
        }

        @Override
        public String toString() {
            return "Opening url of starting page" + mUrl;
        }
    }

    // Creates an action that loads the desired url in the webview upon execution.
    private static class LoadPageAction extends Action {
        private String mUrl;
        private boolean mForce;

        public LoadPageAction(String url, boolean force) {
            mUrl = url;
            mForce = force;
        }

        @Override
        public boolean execute(PerformActions performActions) throws Throwable {
            if (mForce) {
                performActions.loadUrl(mUrl);
            }
            return true;
        }

        @Override
        public String toString() {
            return (mForce ? "Forcing load of" : "Entering") + " new page with url " + mUrl;
        }
    }

    // Creates an action that clicks the given xPath upon execution.
    public static class ClickAction extends Action {
        private String mXPath;

        public ClickAction(String xPath) {
            mXPath = xPath;
        }

        @Override
        public boolean execute(PerformActions performActions) throws Throwable {
            return performActions.selectElement(mXPath);
        }

        @Override
        public String toString() {
            return "Clicking element at xPath " + mXPath;
        }
    }

    // Creates an Action that prompts and selects autofill on the given Xpath upon execution.
    public static class AutofillAction extends Action {
        private String mXPath;

        public AutofillAction(String xPath) {
            mXPath = xPath;
        }

        @Override
        public boolean execute(PerformActions performActions) throws Throwable {
            return performActions.autofill(mXPath);
        }

        @Override
        public String toString() {
            return "Autofilling element at xPath " + mXPath;
        }
    }

    // Creates an Action that checks the given XPath has the given expectedValue upon execution.
    public static class ValidateFieldAction extends Action {
        private String mXPath;
        private String mExpectedValue;

        public ValidateFieldAction(String xPath, String expectedValue) {
            mXPath = xPath;
            mExpectedValue = expectedValue;
        }

        @Override
        public boolean execute(PerformActions performActions) throws Throwable {
            return performActions.verifyAutofill(mXPath, mExpectedValue);
        }

        @Override
        public String toString() {
            return "Verifying element at xPath " + mXPath + " has value " + mExpectedValue;
        }
    }

    public static Action createStartingPageAction(String url) {
        return new StartingPageAction(url);
    }

    public static Action createLoadPageAction(String url, boolean force) {
        return new LoadPageAction(url, force);
    }

    public static Action createClickAction(String xPath) {
        return new ClickAction(xPath);
    }

    public static Action createAutofillAction(String xPath) {
        return new AutofillAction(xPath);
    }

    public static Action createValidateFieldAction(String xPath, String expectedValue) {
        return new ValidateFieldAction(xPath, expectedValue);
    }
}
