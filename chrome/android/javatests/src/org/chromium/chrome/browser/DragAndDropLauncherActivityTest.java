// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.os.Build.VERSION_CODES;

import androidx.annotation.RequiresApi;
import androidx.test.filters.LargeTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.ExecutionException;

/** Tests for {@link DragAndDropLauncherActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "This class tests activity start behavior and thus cannot be batched.")
@MinAndroidSdkLevel(VERSION_CODES.S)
@RequiresApi(VERSION_CODES.S)
public class DragAndDropLauncherActivityTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Context mContext;
    private String mLinkUrl;
    private final CallbackHelper mTabAddedCallback = new CallbackHelper();
    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mContext = ContextUtils.getApplicationContext();
        mLinkUrl = JUnitTestGURLs.EXAMPLE_URL.getSpec();
        mActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
    }

    /**
     * Tests that a dragged link intent is launched in a new Chrome window from
     * DragAndDropLauncherActivity.
     */
    @Test
    @LargeTest
    public void testDragAndDropLauncherActivity_createNewTabbedActivity() throws Exception {
        Intent intent = createLinkDragDropIntent(mLinkUrl, null);
        ChromeTabbedActivity lastAccessedActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> mContext.startActivity(intent));
        mActivityTestRule.setActivity(lastAccessedActivity);

        // Verify that a new Chrome instance is created.
        Assert.assertEquals(
                "Number of Chrome instances should be correct.",
                2,
                MultiWindowUtils.getInstanceCount());

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = mActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(
                            "Activity tab should be non-null.",
                            activityTab,
                            Matchers.notNullValue());
                });

        // Verify that the link is opened in the activity tab of the new Chrome instance.
        Tab activityTab =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.getActivity().getActivityTab());
        Assert.assertEquals(
                "Activity tab URL should match the dragged link URL.",
                new GURL(mLinkUrl).getSpec(),
                ChromeTabUtils.getUrlOnUiThread(activityTab).getSpec());
        Assert.assertTrue(
                "User action should be logged.",
                mActionTester
                        .getActions()
                        .contains(DragAndDropLauncherActivity.LAUNCHED_FROM_LINK_USER_ACTION));
    }

    /**
     * Tests that a dragged link intent is launched in a new tab on the Chrome window with the
     * specified windowId from DragAndDropLauncherActivity when the max number of Chrome instances
     * is open.
     */
    @Test
    @LargeTest
    public void testDragAndDropLauncherActivity_openInExistingTabbedActivity() throws Exception {
        // Simulate creation of max Chrome instances (= 2 for the purpose of testing) and establish
        // the last accessed instance. Actual max # of instances will not be created as this would
        // cause a significant overhead for testing this scenario where a link is opened in an
        // existing instance.
        Intent intent = createLinkDragDropIntent(mLinkUrl, null);
        ChromeTabbedActivity lastAccessedActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> mContext.startActivity(intent));
        mActivityTestRule.setActivity(lastAccessedActivity);
        int lastAccessedInstanceId =
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TabWindowManagerSingleton.getInstance()
                                        .getIndexForWindow(lastAccessedActivity));
        addTabModelSelectorObserver(lastAccessedActivity);

        // Simulate an attempt to open a new window from a dragged link intent when max instances
        // are open. Do this by setting the EXTRA_WINDOW_ID extra on the intent that is reflective
        // of this state.
        Intent newIntent =
                createLinkDragDropIntent(JUnitTestGURLs.MAPS_URL.getSpec(), lastAccessedInstanceId);
        mContext.startActivity(newIntent);
        mTabAddedCallback.waitForCallback(0);

        // Verify that no new Chrome instance is created.
        Assert.assertEquals(
                "Number of Chrome instances should be correct.",
                2,
                MultiWindowUtils.getInstanceCount());

        // Verify that the link is opened in the activity tab of the last accessed Chrome instance.
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = lastAccessedActivity.getActivityTab();
                    Criteria.checkThat(
                            "Activity tab URL should match the dragged link URL.",
                            ChromeTabUtils.getUrlOnUiThread(activityTab).getSpec(),
                            Matchers.is(JUnitTestGURLs.MAPS_URL.getSpec()));
                });
    }

    /**
     * Tests that a dragged link intent is dropped by DragAndDropLauncherActivity when the intent
     * creation timestamp is invalid.
     */
    @Test
    @LargeTest
    public void testDragAndDropLauncherActivity_invalidIntentCreationTimestamp() throws Exception {
        DragAndDropLauncherActivity.setLinkDropTimeoutMsForTesting(500L);
        Intent intent = createLinkDragDropIntent(mLinkUrl, null);
        Thread.sleep(DragAndDropLauncherActivity.getLinkDropTimeoutMs() + 1);
        mContext.startActivity(intent);
        // Verify that no new Chrome instance is created.
        Assert.assertEquals(
                "Number of instances should be correct.", 1, MultiWindowUtils.getInstanceCount());
    }

    private void addTabModelSelectorObserver(ChromeTabbedActivity activity) {
        TabModelSelector tabModelSelector = activity.getTabModelSelector();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelSelectorObserver tabModelSelectorObserver =
                            new TabModelSelectorObserver() {
                                @Override
                                public void onNewTabCreated(
                                        Tab tab, @TabCreationState int creationState) {
                                    mTabAddedCallback.notifyCalled();
                                }
                            };
                    tabModelSelector.addObserver(tabModelSelectorObserver);
                });
    }

    private Intent createLinkDragDropIntent(String linkUrl, Integer windowId)
            throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        DragAndDropLauncherActivity.getLinkLauncherIntent(
                                mContext, linkUrl, windowId));
    }
}
