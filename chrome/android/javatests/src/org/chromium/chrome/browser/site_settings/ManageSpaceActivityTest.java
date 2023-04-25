// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.content.Intent;

import androidx.appcompat.app.AlertDialog;
import androidx.test.InstrumentationRegistry;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.assertion.ViewAssertions;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for ManageSpaceActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManageSpaceActivityTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        if (!mActivityTestRule.getName().equals("testClearUnimporantWithoutChromeStart")) {
            mActivityTestRule.startMainActivityOnBlankPage();
        }
        mTestServer = EmbeddedTestServer.createAndStartServer(
                ApplicationProvider.getApplicationContext());
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
        CriteriaHelper.pollUiThread(() -> activity.getClearUnimportantButton().isEnabled());
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
        CriteriaHelper.pollUiThread(() -> activity.getUnimportantConfirmDialog().isShowing());
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
    public void testLaunchActivity() {
        startManageSpaceActivity().finish();
    }

    @Test
    @MediumTest
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

    @Test
    @MediumTest
    @Feature({"SiteEngagement"})
    public void testManageSiteStorage() {
        ManageSpaceActivity manageSpaceActivity = startManageSpaceActivity();
        waitForClearButtonEnabled(manageSpaceActivity);
        onView(withId(R.id.manage_site_data_storage)).perform(click());
        Espresso.onView(withText("Data stored")).check(ViewAssertions.matches(isDisplayed()));
        manageSpaceActivity.finish();
    }

    // TODO(dmurph): Test the other buttons. One should go to the site storage list, and the other
    //               should reset all app data.
}
