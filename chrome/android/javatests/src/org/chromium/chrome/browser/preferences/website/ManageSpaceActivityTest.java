// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.annotation.TargetApi;
import android.content.Intent;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.app.AlertDialog;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.preferences.privacy.BrowsingDataBridge;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for ManageSpaceActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@TargetApi(Build.VERSION_CODES.KITKAT)
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManageSpaceActivityTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        if (!mActivityTestRule.getName().equals("testClearUnimporantWithoutChromeStart")) {
            mActivityTestRule.startMainActivityOnBlankPage();
        }
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private ManageSpaceActivity startManageSpaceActivity() {
        Intent intent =
                new Intent(InstrumentationRegistry.getTargetContext(), ManageSpaceActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return (ManageSpaceActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                intent);
    }

    public void waitForClearButtonEnabled(final ManageSpaceActivity activity) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return activity.getClearUnimportantButton().isEnabled();
            }
        });
    }

    public Runnable getClickClearRunnable(final ManageSpaceActivity activity) {
        return new Runnable() {
            @Override
            public void run() {
                activity.onClick(activity.getClearUnimportantButton());
            }
        };
    }

    public void waitForDialogShowing(final ManageSpaceActivity activity) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return activity.getUnimportantConfirmDialog().isShowing();
            }
        });
    }

    public Runnable getPressClearRunnable(final AlertDialog dialog) {
        return new Runnable() {
            @Override
            public void run() {
                dialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
            }
        };
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testLaunchActivity() {
        startManageSpaceActivity();
    }

    @Test
    @MediumTest
    @RetryOnFailure
    @Feature({"SiteEngagement"})
    public void testClearUnimportantOnly() throws Exception {
        final String cookiesUrl =
                mTestServer.getURL("/chrome/test/data/android/storage_persistance.html");
        final String serverOrigin = mTestServer.getURL("/");

        mActivityTestRule.loadUrl(cookiesUrl + "#clear");
        Assert.assertEquals(
                "false", mActivityTestRule.runJavaScriptCodeInCurrentTab("hasAllStorage()"));
        mActivityTestRule.runJavaScriptCodeInCurrentTab("setStorage()");
        Assert.assertEquals(
                "true", mActivityTestRule.runJavaScriptCodeInCurrentTab("hasAllStorage()"));
        mActivityTestRule.loadUrl("about:blank");

        // Now we set the origin as important, and check that we don't clear it.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { BrowsingDataBridge.markOriginAsImportantForTesting(serverOrigin); });

        ManageSpaceActivity manageSpaceActivity = startManageSpaceActivity();
        // Click 'clear' in the CBD screen.
        waitForClearButtonEnabled(manageSpaceActivity);
        TestThreadUtils.runOnUiThreadBlocking(getClickClearRunnable(manageSpaceActivity));
        // Press 'clear' in our dialog.
        waitForDialogShowing(manageSpaceActivity);
        TestThreadUtils.runOnUiThreadBlocking(
                getPressClearRunnable(manageSpaceActivity.getUnimportantConfirmDialog()));
        waitForClearButtonEnabled(manageSpaceActivity);
        manageSpaceActivity.finish();

        mActivityTestRule.loadUrl(cookiesUrl);
        Assert.assertEquals(
                "true", mActivityTestRule.runJavaScriptCodeInCurrentTab("hasAllStorage()"));
    }

    @Test
    @MediumTest
    @Feature({"SiteEngagement"})
    public void testClearUnimporantWithoutChromeStart() {
        ManageSpaceActivity manageSpaceActivity = startManageSpaceActivity();
        // Click 'clear' in the CBD screen.
        waitForClearButtonEnabled(manageSpaceActivity);
        TestThreadUtils.runOnUiThreadBlocking(getClickClearRunnable(manageSpaceActivity));
        // Press 'clear' in our dialog.
        waitForDialogShowing(manageSpaceActivity);
        TestThreadUtils.runOnUiThreadBlocking(
                getPressClearRunnable(manageSpaceActivity.getUnimportantConfirmDialog()));
        waitForClearButtonEnabled(manageSpaceActivity);
        manageSpaceActivity.finish();
    }

    // TODO(dmurph): Test the other buttons. One should go to the site storage list, and the other
    //               should reset all app data.
}
