// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test.util;

import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.endsWith;
import static org.hamcrest.Matchers.hasItem;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.content.Intent;
import android.webkit.WebView;

import androidx.test.espresso.BaseLayerComponent;
import androidx.test.espresso.DaggerBaseLayerComponent;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.webview_ui_test.R;
import org.chromium.webview_ui_test.WebViewUiTestActivity;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * WebViewUiTestRule provides ways to synchronously loads file URL or javascript.
 *
 * Note that this must be run on test thread.
 *
 */
public class WebViewUiTestRule extends BaseActivityTestRule<WebViewUiTestActivity> {
    private static final long ACTION_BAR_POPUP_TIMEOUT = scaleTimeout(5000L);
    private static final long ACTION_BAR_CHECK_INTERVAL = 200L;

    private WebViewSyncWrapper mSyncWrapper;
    private String mLayout;
    private BaseLayerComponent mBaseLayerComponent;

    public WebViewUiTestRule(Class<WebViewUiTestActivity> activityClass) {
        super(activityClass);
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
    public void launchActivity(Intent i) {
        if (mLayout != null && !mLayout.isEmpty()) {
            if (i == null) i = getActivityIntent();
            i.putExtra(WebViewUiTestActivity.EXTRA_TEST_LAYOUT_FILE, mLayout);
        }
        super.launchActivity(i);
        mSyncWrapper = new WebViewSyncWrapper((WebView) getActivity().findViewById(R.id.webview));
    }

    public void launchActivity() {
        launchActivity(null);
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
            ThreadUtils.runOnUiThreadBlocking(() -> isDisplayed.set(isActionBarDisplayedFunc()));
        } catch (Throwable e) {
            throw new RuntimeException("Exception while checking action bar", e);
        }
        return isDisplayed.get();
    }

    private boolean isActionBarDisplayedFunc() {
        if (mBaseLayerComponent == null) mBaseLayerComponent = DaggerBaseLayerComponent.create();

        if (hasItem(
                        withDecorView(
                                withChild(
                                        allOf(
                                                withClassName(endsWith("PopupBackgroundView")),
                                                isCompletelyDisplayed()))))
                .matches(mBaseLayerComponent.activeRootLister().listActiveRoots())) {
            return true;
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
