// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test.util;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.actionWithAssertions;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;

import static org.hamcrest.CoreMatchers.allOf;

import android.view.View;
import android.webkit.WebView;

import androidx.annotation.VisibleForTesting;
import androidx.test.espresso.PerformException;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.action.CoordinatesProvider;
import androidx.test.espresso.action.GeneralClickAction;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Tap;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.uiautomator.By;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;

import org.chromium.base.Log;

import java.util.concurrent.TimeUnit;

/** Helper functions returning {@link ViewAction}s that can be performed on a {@link WebView}. */
public class PerformActions {
    private UiDevice mDevice;
    private CapturedSitesTestRule mWebViewActivityRule;
    private static final String TAG = "PerformActions";
    private static final int TIME_BETWEEN_ACTIONS_SECONDS = 2;
    private static final double TIMEOUT_SECONDS = 10;
    private static final double POLLING_INTERVAL_SECONDS = 0.1;
    private static final int ACTION_RETRIES = 2;
    private static final int MARGIN =
            49; // TODO (crbug/1470296) Replace with automatic margin detection.

    /** Maps between relative [0, 1] coordinates to screen size. */
    public static class ElementCoordinates implements CoordinatesProvider {
        private final double mX;
        private final double mY;

        private ElementCoordinates(double x, double y) {
            this.mX = x;
            this.mY = y;
            Log.d(
                    TAG,
                    "ElementCoordinates: x = "
                            + Double.toString(this.mX)
                            + ", y = "
                            + Double.toString(this.mY));
        }

        @Override
        public float[] calculateCoordinates(View view) {
            final int[] xy = {0, 0};
            view.getLocationOnScreen(xy);
            Log.d(
                    TAG,
                    "WebView: top = "
                            + Integer.toString(view.getTop())
                            + ", left = "
                            + Integer.toString(view.getLeft())
                            + ", bottom = "
                            + Integer.toString(view.getBottom())
                            + ", right = "
                            + Integer.toString(view.getRight())
                            + ", width = "
                            + Integer.toString(view.getWidth())
                            + ", height ="
                            + Integer.toString(view.getHeight()));
            xy[0] = (int) (mX * view.getWidth());
            xy[1] = (int) (mY * view.getHeight());
            Log.d(TAG, "x = " + Integer.toString(xy[0]) + ", y = " + Integer.toString(xy[1]));
            return new float[] {xy[0], xy[1]};
        }
    }

    public PerformActions(UiDevice device, CapturedSitesTestRule webViewActivityRule) {
        this.mDevice = device;
        this.mWebViewActivityRule = webViewActivityRule;
    }

    // Loads the webpage at the given url into the webview.
    public void loadUrl(String url) throws Exception {
        TimeUnit.SECONDS.sleep(TIME_BETWEEN_ACTIONS_SECONDS);
        mWebViewActivityRule.loadUrlSync(url);
    }

    // Clicks on the element at the given id.
    public boolean selectElement(String xPath) throws Throwable {
        TimeUnit.SECONDS.sleep(TIME_BETWEEN_ACTIONS_SECONDS);
        ViewInteraction myView = onMyWebView();
        scrollToElement(xPath);
        TimeUnit.SECONDS.sleep(TIME_BETWEEN_ACTIONS_SECONDS);
        double elemX = getWidthRelative(xPath);
        double elemY = getHeightRelative(xPath);
        try {
            myView.perform(
                    actionWithAssertions(
                            new GeneralClickAction(
                                    Tap.SINGLE,
                                    new ElementCoordinates(elemX, elemY),
                                    Press.FINGER)));
            return true;
        } catch (PerformException e) {
            Log.e(TAG, "Could not select" + e.getMessage());
            return false;
        }
    }

    // Clicks on the field at the given xPath, then select the autofill pop-up box.
    public boolean autofill(String xPath) throws Throwable {
        TimeUnit.SECONDS.sleep(TIME_BETWEEN_ACTIONS_SECONDS);
        selectElement(xPath);
        TimeUnit.SECONDS.sleep(TIME_BETWEEN_ACTIONS_SECONDS);
        UiObject2 firstAutofill =
                mDevice.findObject(By.res("org.chromium.webview_ui_test", "text"));
        if (firstAutofill == null) {
            Log.d(TAG, "Autofill element was not found");
            return false;
        } else {
            Log.d(TAG, "Autofill element was found");
            firstAutofill.click();
            return true;
        }
    }

    // Checks that the field at the given Xpath matches the expected result after autofill.
    public boolean verifyAutofill(String xPath, String expected) throws Throwable {
        TimeUnit.SECONDS.sleep(TIME_BETWEEN_ACTIONS_SECONDS);
        String js = getElemToXPath(xPath) + "elem.value;";
        String callback = findCallback(js);
        callback = callback.substring(1, callback.length() - 1);
        Log.d(TAG, "Javascript Callback: " + callback);
        return callback.equals(expected);
    }

    // Runs javascript command to scroll element into view.
    @VisibleForTesting
    void scrollToElement(String xPath) throws Throwable {
        String js = getElemToXPath(xPath) + "elem.scrollIntoView();";
        onMyWebView().perform(this.getViewAction(js));
    }

    // Returns the relative height [0, 1] of the given element on the current webview.
    @VisibleForTesting
    double getHeightRelative(String xPath) throws Throwable {
        final String topJS = getElemToXPath(xPath) + "elem.getBoundingClientRect().top;";
        final String bottomJS =
                getElemToXPath(xPath)
                        + "elem.getBoundingClientRect().bottom + "
                        + (MARGIN * 2) // TODO (crbug/1470296): Replace with margin detection.
                        + ";";
        final String heightJS = "window.innerHeight";
        return getRelativePos(topJS, bottomJS, heightJS, xPath);
    }

    // Returns the relative width [0, 1] of the given element on the current webview.
    @VisibleForTesting
    double getWidthRelative(String xPath) throws Throwable {
        final String leftJS = getElemToXPath(xPath) + "elem.getBoundingClientRect().left;";
        final String rightJS = getElemToXPath(xPath) + "elem.getBoundingClientRect().right;";
        final String widthJS = "window.innerWidth";
        return getRelativePos(leftJS, rightJS, widthJS, xPath);
    }

    // Generalizable helper for width and height that computes where the element lies relative to
    // webview.
    @VisibleForTesting
    double getRelativePos(String lowerJS, String higherJS, String sizeJS, String xPath)
            throws Throwable {
        String errorString = "No element found with xPath: " + xPath;

        String lowerString = findCallbackAndFailIfNull(lowerJS, errorString);
        double lower = Double.valueOf(lowerString);

        String higherString = findCallbackAndFailIfNull(higherJS, errorString);
        double higher = Double.valueOf(higherString);

        String sizeString = findCallbackAndFailIfNull(sizeJS);
        double size = Double.valueOf(sizeString);

        return ((lower + higher) / 2) / size;
    }

    // Runs javascript and returns callback if present, fails with default error message otherwise.
    @VisibleForTesting
    String findCallbackAndFailIfNull(String js) throws Throwable {
        String errorMessage = "from javascript " + js;
        return findCallbackAndFailIfNull(js, errorMessage);
    }

    // Runs javascript and returns callback if present, fails with message otherwise.
    @VisibleForTesting
    String findCallbackAndFailIfNull(String js, String errorMessage) throws Throwable {
        String callback = findCallback(js);
        // Checks if there was no callback, or the result of callback was itself null.
        if (callback == null || callback.equals("null")) {
            throw new NullPointerException("Callback failed:" + errorMessage);
        }
        return callback;
    }

    // Sets the element at xPath to a JS variable elem.
    @VisibleForTesting
    String getElemToXPath(String xPath) {
        xPath = addEscapes(xPath);
        return "function getElementByXpath(path) {"
                + "return document.evaluate"
                + "(path, document, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null)"
                + ".singleNodeValue;}"
                + "var elem = "
                + "getElementByXpath(\""
                + xPath
                + "\");";
    }

    // Adds necessary backlashes to escape quotes in Javascript code.
    String addEscapes(String xPath) {
        return xPath.replace("\"", "\\\"");
    }

    // Runs the given javascript command and returns the callback if there was one.
    // Should only be used if expecting callback.
    @VisibleForTesting
    String findCallback(String js) throws Throwable {
        JavaScriptExecutionViewAction action = this.getViewAction(js);
        if (!waitForCallback(action)) {
            return null;
        }
        return action.callback.returnValue;
    }

    // Attempts to perform the given action multiple times, checking for callback.
    // Returns true if there was a callback, or false if there was not.
    @VisibleForTesting
    boolean waitForCallback(JavaScriptExecutionViewAction action) throws Throwable {
        for (int i = 0; i < ACTION_RETRIES; i++) {
            onMyWebView().perform(action);
            for (double poll = 0; poll < TIMEOUT_SECONDS; poll += POLLING_INTERVAL_SECONDS) {
                if (action.callback.returnValue != null) {
                    return true;
                }
                TimeUnit.MILLISECONDS.sleep((int) (1000 * POLLING_INTERVAL_SECONDS));
            }
        }
        return false;
    }

    // Get a ViewInteraction that takes place on the current Webview.
    @VisibleForTesting
    ViewInteraction onMyWebView() {
        return onView(
                allOf(
                        isAssignableFrom(WebView.class),
                        withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
    }

    /**
     * Returns a {@link ViewAction} for the given JavaScript.
     *
     * @param jsString JavaScript string
     * @return {@link ViewAction} for using on a {@link WebView}.
     */
    @VisibleForTesting
    JavaScriptExecutionViewAction getViewAction(String jsString) {
        return new JavaScriptExecutionViewAction(jsString);
    }
}
