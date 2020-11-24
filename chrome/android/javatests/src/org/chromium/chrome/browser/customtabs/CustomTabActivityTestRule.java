// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.FeatureList;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;

import java.util.Collections;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Custom ActivityTestRule for all instrumentation tests that require a {@link CustomTabActivity}.
 */
public class CustomTabActivityTestRule extends ChromeActivityTestRule<CustomTabActivity> {
    protected static final long STARTUP_TIMEOUT_MS = ScalableTimeout.scaleTimeout(5L * 1000);
    protected static final long LONG_TIMEOUT_MS = 10L * 1000;
    private static int sCustomTabId;

    public CustomTabActivityTestRule() {
        super(CustomTabActivity.class);
    }

    public static void putCustomTabIdInIntent(Intent intent) {
        boolean hasCustomTabId = intent.hasExtra(CustomTabsTestUtils.EXTRA_CUSTOM_TAB_ID);
        // Intent already has a custom tab id assigned to it and we should reuse the same activity.
        // Test relying on sending the same intent relies on using the same activity.
        if (hasCustomTabId) return;

        intent.putExtra(CustomTabsTestUtils.EXTRA_CUSTOM_TAB_ID, sCustomTabId++);
    }

    public static int getCustomTabIdFromIntent(Intent intent) {
        return intent.getIntExtra(CustomTabsTestUtils.EXTRA_CUSTOM_TAB_ID, -1);
    }

    @Override
    public void startActivityCompletely(Intent intent) {
        if (!FeatureList.hasTestFeatures()) {
            FeatureList.setTestFeatures(
                    Collections.singletonMap(ChromeFeatureList.SHARE_BY_DEFAULT_IN_CCT, true));
        }
        putCustomTabIdInIntent(intent);
        int currentIntentId = getCustomTabIdFromIntent(intent);

        Activity activity = InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
        Assert.assertNotNull("Main activity did not start", activity);
        CriteriaHelper.pollUiThread(() -> {
            for (Activity runningActivity : ApplicationStatus.getRunningActivities()) {
                if (runningActivity instanceof CustomTabActivity) {
                    CustomTabActivity customTabActivity = (CustomTabActivity) runningActivity;
                    final int customTabIdInActivity =
                            getCustomTabIdFromIntent(customTabActivity.getIntent());
                    if (currentIntentId != customTabIdInActivity) continue;
                    setActivity(customTabActivity);
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
    public void startCustomTabActivityWithIntent(Intent intent) {
        DeferredStartupHandler.setExpectingActivityStartupForTesting();
        startActivityCompletely(intent);
        waitForActivityNativeInitializationComplete();
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(getActivity().getActivityTab(), Matchers.notNullValue());
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
        Assert.assertTrue("Deferred startup never completed",
                DeferredStartupHandler.waitForDeferredStartupCompleteForTesting(
                        STARTUP_TIMEOUT_MS));
        Assert.assertNotNull(tab);
        Assert.assertNotNull(tab.getView());
        Assert.assertTrue(TabTestUtils.isCustomTab(tab));
    }
}
