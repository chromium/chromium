// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.when;

import android.content.Intent;

import androidx.annotation.NonNull;

import org.junit.Assert;
import org.mockito.Mockito;

import org.chromium.base.Log;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Custom ActivityTestRule for all instrumentation tests that require a {@link CustomTabActivity}.
 */
public class CustomTabActivityTestRule extends ChromeActivityTestRule<CustomTabActivity> {
    protected static final long LONG_TIMEOUT_MS = 10L * 1000;
    private static final String TAG = "CustomTabTestRule";
    private static int sCustomTabId;

    public CustomTabActivityTestRule() {
        super(CustomTabActivity.class);
    }

    @Override
    protected void before() throws Throwable {
        super.before();
        // TODO(crbug.com/342240475): Find a better way to deal with IPH in tests.
        Log.w(
                TAG,
                "A mock Tracker is set in CustomTabActivityTestRule. This will"
                        + " prevent any IPH from showing. See crbug.com/342240475.");
        Tracker tracker = Mockito.mock(Tracker.class);
        // Disable IPH to prevent it from interfering with the tests.
        when(tracker.shouldTriggerHelpUI(anyString())).thenReturn(false);
        TrackerFactory.setTrackerForTests(tracker);
    }

    public static void putCustomTabIdInIntent(Intent intent) {
        boolean hasCustomTabId = intent.hasExtra(CustomTabsTestUtils.EXTRA_CUSTOM_TAB_ID);
        // Intent already has a custom tab id assigned to it and we should reuse the same activity.
        // Test relying on sending the same intent relies on using the same activity.
        if (hasCustomTabId) return;

        intent.putExtra(CustomTabsTestUtils.EXTRA_CUSTOM_TAB_ID, sCustomTabId++);
    }

    @Override
    public void launchActivity(@NonNull Intent intent) {
        putCustomTabIdInIntent(intent);
        super.launchActivity(intent);
    }

    /**
     * Start a {@link CustomTabActivity} with given {@link Intent}, and wait till a tab is
     * initialized and the first frame is drawn.
     */
    public void startCustomTabActivityWithIntent(Intent intent) {
        startCustomTabActivityWithIntentNotWaitingForFirstFrame(intent);
        waitForFirstFrame();
    }

    /**
     * Start a {@link CustomTabActivity} with given {@link Intent}, and wait till a tab is
     * initialized.
     */
    public void startCustomTabActivityWithIntentNotWaitingForFirstFrame(Intent intent) {
        startActivityCompletely(intent);
        final Tab tab = getActivity().getActivityTab();
        Assert.assertTrue(TabTestUtils.isCustomTab(tab));
    }
}
