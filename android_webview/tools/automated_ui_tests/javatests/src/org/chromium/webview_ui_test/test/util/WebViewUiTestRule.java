// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test.util;

import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.hasDescendant;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withChild;
import static android.support.test.espresso.matcher.ViewMatchers.withClassName;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.endsWith;
import static org.hamcrest.Matchers.hasItem;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.content.Intent;
import android.os.Build;
import android.support.test.espresso.BaseLayerComponent;
import android.support.test.espresso.DaggerBaseLayerComponent;
import android.support.test.rule.ActivityTestRule;
import android.webkit.WebView;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.webview_ui_test.R;
import org.chromium.webview_ui_test.WebViewUiTestActivity;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * WebViewUiTestRule provides ways to synchronously loads file URL or javascript.
 *
 * Note that this must be run on test thread.
 *
 */
public class WebViewUiTestRule extends ActivityTestRule<WebViewUiTestActivity> {
    private static final long ACTION_BAR_POPUP_TIMEOUT = scaleTimeout(5000L);
    private static final long ACTION_BAR_CHECK_INTERVAL = 200L;

    private WebViewSyncWrapper mSyncWrapper;
    private String mLayout;
    private BaseLayerComponent mBaseLayerComponent;

    public WebViewUiTestRule(Class<WebViewUiTestActivity> activityClass) {
        super(activityClass);
    }

    @Override
    protected void afterActivityLaunched() {
        mSyncWrapper = new WebViewSyncWrapper((WebView) getActivity().findViewById(R.id.webview));
        super.afterActivityLaunched();
    }

    @Override
    public Statement apply(Statement base, Description desc) {
        UseLayout a = desc.getAnnotation(UseLayout.class);
        if (a != null) {
            mLayout = a.value();
        }
        return super.apply(base, desc);
    }

    @Override
    public WebViewUiTestActivity launchActivity(Intent i) {
        if (mLayout != null && !mLayout.isEmpty()) {
            i.putExtra(WebViewUiTestActivity.EXTRA_TEST_LAYOUT_FILE, mLayout);
        }
        return super.launchActivity(i);
    }

    public WebViewUiTestActivity launchActivity() {
        return launchActivity(new Intent());
    }

    public void loadDataSync(
            String data, String mimeType, String encoding, boolean confirmByJavaScript) {
        mSyncWrapper.loadDataSync(data, mimeType, encoding, confirmByJavaScript);
    }

    public void loadJavaScriptSync(String js, boolean appendConfirmationJavascript) {
        mSyncWrapper.loadJavaScriptSync(js, appendConfirmationJavascript);
    }

    public void loadFileSync(String html, boolean confirmByJavaScript) {
        mSyncWrapper.loadFileSync(html, confirmByJavaScript);
    }

    /**
     * Wait until the action bar is detected, or timeout occurs
     *
     * Using polling instead of idling resource because on L, the "Paste" option
     * will disappear after a few seconds, too short for the idling resource
     * check interval of 5 seconds to work reliably.
     */
    public boolean waitForActionBarPopup() {
        long startTime = System.currentTimeMillis();
        while (System.currentTimeMillis() - startTime < ACTION_BAR_POPUP_TIMEOUT) {
            if (isActionBarDisplayed()) {
                sleep(ACTION_BAR_CHECK_INTERVAL);
                if (isActionBarDisplayed()) {
                    return true;
                }
            }
            sleep(ACTION_BAR_CHECK_INTERVAL);
        }
        return false;
    }

    public boolean isActionBarDisplayed() {
        final AtomicBoolean isDisplayed = new AtomicBoolean(false);
        try {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    isDisplayed.set(isActionBarDisplayedFunc());
                }
            });
        } catch (Throwable e) {
            throw new RuntimeException("Exception while checking action bar", e);
        }
        return isDisplayed.get();
    }

    private boolean isActionBarDisplayedFunc() {
        if (mBaseLayerComponent == null) mBaseLayerComponent = DaggerBaseLayerComponent.create();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // For M and above
            if (hasItem(withDecorView(withChild(allOf(
                    withClassName(endsWith("PopupBackgroundView")),
                    isCompletelyDisplayed())))).matches(
                    mBaseLayerComponent.activeRootLister().listActiveRoots())) {
                return true;
            }
        } else {
            // For L
            if (hasItem(withDecorView(hasDescendant(allOf(
                    withClassName(endsWith("ActionMenuItemView")),
                    isCompletelyDisplayed())))).matches(
                    mBaseLayerComponent.activeRootLister().listActiveRoots())) {
                return true;
            }

            // Paste option is a popup on L
            if (hasItem(withDecorView(withChild(withText("Paste")))).matches(
                    mBaseLayerComponent.activeRootLister().listActiveRoots())) {
                return true;
            }
        }


        return false;
    }

    private void sleep(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
        }
    }
}
