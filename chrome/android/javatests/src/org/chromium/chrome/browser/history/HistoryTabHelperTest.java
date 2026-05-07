// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.provider.Browser;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

/** Tests for history tab helper. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class HistoryTabHelperTest {
    private static final String FILE_PATH = "/chrome/test/data/android/test.html";
    private static final String FILE_PATH2 = "/chrome/test/data/android/simple.html";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Test
    @MediumTest
    @DisableIf.Build(
            sdk_is_less_than = VERSION_CODES.UPSIDE_DOWN_CAKE,
            message = "This test is using an API introduced in Android U.")
    public void testAppHistory() throws Exception {
        Context context = ContextUtils.getApplicationContext();
        Intent viewIntent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(mActivityTestRule.getTestServer().getURL(FILE_PATH)));
        viewIntent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        viewIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        viewIntent.setClass(context, ChromeLauncherActivity.class);
        Bundle bundle = ActivityOptions.makeBasic().setShareIdentityEnabled(true).toBundle();
        ChromeTabbedActivity resultActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () -> context.startActivity(viewIntent, bundle));

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = getCurrentTab(resultActivity);
                    return tab != null && HistoryTabHelper.get(tab) != null;
                },
                "Timed out while waiting for the current Tab.");
        Assert.assertEquals(
                "App ID should be set",
                resultActivity.getApplicationContext().getPackageName(),
                getAppId(resultActivity));

        // Navigation after the initial loading.
        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = getCurrentTab(resultActivity);
                    tab.addObserver(
                            new EmptyTabObserver() {
                                @Override
                                public void onPageLoadFinished(Tab tab, GURL url) {
                                    callbackHelper.notifyCalled();
                                }
                            });
                    var gurl = mActivityTestRule.getTestServer().getURL(FILE_PATH2);
                    tab.loadUrl(new LoadUrlParams(gurl));
                });
        callbackHelper.waitForCallback(0);
        Assert.assertNull("App ID should be reset from the second visit", getAppId(resultActivity));
    }

    private Tab getCurrentTab(ChromeTabbedActivity activity) {
        return activity.getActivityTabProvider().get();
    }

    private String getAppId(ChromeTabbedActivity activity) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        HistoryTabHelper.getAppIdForTesting(
                                getCurrentTab(activity).getWebContents()));
    }
}
