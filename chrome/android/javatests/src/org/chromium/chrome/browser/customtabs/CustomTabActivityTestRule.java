// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.app.Activity;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.lang.ref.WeakReference;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Custom ActivityTestRule for all instrumentation tests that require a {@link CustomTabActivity}.
 */
public class CustomTabActivityTestRule extends ChromeActivityTestRule<CustomTabActivity> {
    protected static final long STARTUP_TIMEOUT_MS = scaleTimeout(5) * 1000;
    protected static final long LONG_TIMEOUT_MS = scaleTimeout(10) * 1000;

    public CustomTabActivityTestRule() {
        super(CustomTabActivity.class);
    }

    @Override
    public void startActivityCompletely(Intent intent) {
        Activity activity = InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
        Assert.assertNotNull("Main activity did not start", activity);
        CriteriaHelper.pollUiThread(() -> {
            for (WeakReference<Activity> ref : ApplicationStatus.getRunningActivities()) {
                Activity runningActivity = ref.get();
                if (runningActivity instanceof CustomTabActivity) {
                    setActivity((CustomTabActivity) runningActivity);
                    return true;
                }
            }
            return false;
        });
    }

    /**
     * Start a {@link CustomTabActivity} with given {@link Intent}, and wait till a tab is
     * initialized.
     */
    public void startCustomTabActivityWithIntent(Intent intent) throws InterruptedException {
        startActivityCompletely(intent);
        waitForActivityNativeInitializationComplete();
        CriteriaHelper.pollUiThread(new Criteria("Tab never selected/initialized.") {
            @Override
            public boolean isSatisfied() {
                return getActivity().getActivityTab() != null;
            }
        });
        final Tab tab = getActivity().getActivityTab();
        final CallbackHelper pageLoadFinishedHelper = new CallbackHelper();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                pageLoadFinishedHelper.notifyCalled();
            }
        });
        try {
            if (tab.isLoading()) {
                pageLoadFinishedHelper.waitForCallback(
                        0, 1, LONG_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            }
        } catch (TimeoutException e) {
            Assert.fail();
        }
        CriteriaHelper.pollUiThread(
                DeferredStartupHandler.getInstance()::isDeferredStartupCompleteForApp,
                "Deferred startup never completed", STARTUP_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        Assert.assertNotNull(tab);
        Assert.assertNotNull(tab.getView());
        Assert.assertTrue(tab.isCurrentlyACustomTab());
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return super.apply(base, description);
    }
}
