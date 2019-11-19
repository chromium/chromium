// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreTest.MockTabPersistentStoreObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Tests merging tab models for Android N+ multi-instance.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@TargetApi(Build.VERSION_CODES.N)
@MinAndroidSdkLevel(Build.VERSION_CODES.N)
public class TabModelMergingTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_URL_0 = UrlUtils.encodeHtmlDataUri("<html>test_url_0.</html>");
    private static final String TEST_URL_1 = UrlUtils.encodeHtmlDataUri("<html>test_url_1.</html>");
    private static final String TEST_URL_2 = UrlUtils.encodeHtmlDataUri("<html>test_url_2.</html>");
    private static final String TEST_URL_3 = UrlUtils.encodeHtmlDataUri("<html>test_url_3.</html>");
    private static final String TEST_URL_4 = UrlUtils.encodeHtmlDataUri("<html>test_url_4.</html>");
    private static final String TEST_URL_5 = UrlUtils.encodeHtmlDataUri("<html>test_url_5.</html>");
    private static final String TEST_URL_6 = UrlUtils.encodeHtmlDataUri("<html>test_url_6.</html>");
    private static final String TEST_URL_7 = UrlUtils.encodeHtmlDataUri("<html>test_url_7.</html>");

    private ChromeTabbedActivity mActivity1;
    private ChromeTabbedActivity mActivity2;
    private int mActivity1State;
    private int mActivity2State;
    private String[] mMergeIntoActivity1ExpectedTabs;
    private String[] mMergeIntoActivity2ExpectedTabs;

    CallbackHelper mNewCTA2CallbackHelper = new CallbackHelper();
    private ChromeTabbedActivity2 mNewCTA2;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        // Make sure file migrations don't run as they are unnecessary since app data was cleared.
        ContextUtils.getAppSharedPreferences().edit().putBoolean(
                TabbedModeTabPersistencePolicy.PREF_HAS_RUN_FILE_MIGRATION, true).apply();
        ContextUtils.getAppSharedPreferences().edit().putBoolean(
                TabbedModeTabPersistencePolicy.PREF_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION, true)
                        .apply();

        // Some of the logic for when to trigger a merge depends on whether the activity is in
        // multi-window mode. Set isInMultiWindowMode to true to avoid merging unexpectedly.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);

        // Initialize activities.
        mActivity1 = mActivityTestRule.getActivity();
        // Start multi-instance mode so that ChromeTabbedActivity's check for whether the activity
        // is started up correctly doesn't fail.
        MultiInstanceManager.onMultiInstanceModeStarted();
        mActivity2 = MultiWindowTestHelper.createSecondChromeTabbedActivity(
                mActivity1, new LoadUrlParams(TEST_URL_7));
        CriteriaHelper.pollUiThread(new Criteria("CTA2 tab state failed to initialize.") {
            @Override
            public boolean isSatisfied() {
                return mActivity2.areTabModelsInitialized()
                        && mActivity2.getTabModelSelector().isTabStateInitialized();
            }
        });

        // Create a few tabs in each activity.
        createTabsOnUiThread();

        // Initialize activity states and register for state change events.
        mActivity1State = ApplicationStatus.getStateForActivity(mActivity1);
        mActivity2State = ApplicationStatus.getStateForActivity(mActivity2);
        ApplicationStatus.registerStateListenerForAllActivities(new ActivityStateListener() {
            @Override
            public void onActivityStateChange(Activity activity, int newState) {
                if (activity.equals(mActivity1)) {
                    mActivity1State = newState;
                } else if (activity.equals(mActivity2)) {
                    mActivity2State = newState;
                } else if (activity instanceof ChromeTabbedActivity2
                        && newState == ActivityState.CREATED) {
                    mNewCTA2 = (ChromeTabbedActivity2) activity;
                    mNewCTA2CallbackHelper.notifyCalled();
                }
            }
        });
    }

    /**
     * Creates new tabs in both ChromeTabbedActivity and ChromeTabbedActivity2 and asserts that each
     * has the expected number of tabs.
     */
    private void createTabsOnUiThread() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Create normal tabs.
            mActivity1.getTabCreator(false).createNewTab(
                    new LoadUrlParams(TEST_URL_0), TabLaunchType.FROM_CHROME_UI, null);
            mActivity1.getTabCreator(false).createNewTab(
                    new LoadUrlParams(TEST_URL_1), TabLaunchType.FROM_CHROME_UI, null);
            mActivity1.getTabCreator(false).createNewTab(
                    new LoadUrlParams(TEST_URL_2), TabLaunchType.FROM_CHROME_UI, null);
            mActivity2.getTabCreator(false).createNewTab(
                    new LoadUrlParams(TEST_URL_3), TabLaunchType.FROM_CHROME_UI, null);
            mActivity2.getTabCreator(false).createNewTab(
                    new LoadUrlParams(TEST_URL_4), TabLaunchType.FROM_CHROME_UI, null);

            mActivity1.saveState();
            mActivity2.saveState();
        });

        // ChromeTabbedActivity should have four normal tabs, the one it started with and the three
        // just created.
        Assert.assertEquals("Wrong number of tabs in ChromeTabbedActivity", 4,
                mActivity1.getTabModelSelector().getModel(false).getCount());

        // ChromeTabbedActivity2 should have three normal tabs, the one it started with and the two
        // just created.
        Assert.assertEquals("Wrong number of tabs in ChromeTabbedActivity2", 3,
                mActivity2.getTabModelSelector().getModel(false).getCount());

        // Construct expected tabs.
        mMergeIntoActivity1ExpectedTabs = new String[7];
        mMergeIntoActivity2ExpectedTabs = new String[7];
        for (int i = 0; i < 4; i++) {
            mMergeIntoActivity1ExpectedTabs[i] =
                    mActivity1.getTabModelSelector().getModel(false).getTabAt(i).getUrl();
            mMergeIntoActivity2ExpectedTabs[i + 3] =
                    mActivity1.getTabModelSelector().getModel(false).getTabAt(i).getUrl();
        }
        for (int i = 0; i < 3; i++) {
            mMergeIntoActivity2ExpectedTabs[i] =
                    mActivity2.getTabModelSelector().getModel(false).getTabAt(i).getUrl();
            mMergeIntoActivity1ExpectedTabs[i + 4] =
                    mActivity2.getTabModelSelector().getModel(false).getTabAt(i).getUrl();
        }
    }

    private void mergeTabsAndAssert(final ChromeTabbedActivity activity,
            final String[] expectedTabUrls) {
        String selectedTabUrl = activity.getTabModelSelector().getCurrentTab().getUrl();
        mergeTabsAndAssert(activity, expectedTabUrls, expectedTabUrls.length, selectedTabUrl);
    }

    /**
     * Merges tabs into the provided activity and asserts that the tab model looks as expected.
     * @param activity The activity to merge into.
     * @param expectedTabUrls The expected ordering of normal tab URLs after the merge.
     * @param expectedNumberOfTabs The expected number of tabs after the merge.
     * @param expectedSelectedTabUrl The expected URL of the selected tab after the merge.
     */
    private void mergeTabsAndAssert(final ChromeTabbedActivity activity,
            final String[] expectedTabUrls, final int expectedNumberOfTabs,
            String expectedSelectedTabUrl) {
        // Merge tabs into the activity.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.getMultiInstanceMangerForTesting().maybeMergeTabs());

        // Wait for all tabs to be merged into the activity.
        CriteriaHelper.pollUiThread(new Criteria("Total tab count incorrect.") {
            @Override
            public boolean isSatisfied() {
                return activity.getTabModelSelector().getTotalTabCount() == expectedNumberOfTabs;
            }
        });

        assertTabModelMatchesExpectations(activity, expectedSelectedTabUrl, expectedTabUrls);
    }

    /**
     * @param context The context to use when creating the intent.
     * @return An intent that can be used to start a new ChromeTabbedActivity.
     */
    private Intent createChromeTabbedActivityIntent(Context context) {
        Intent intent = new Intent();
        intent.setClass(context, ChromeTabbedActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    /**
     * Starts a new ChromeTabbedActivity and asserts that the tab model looks as expected.
     * @param intent The intent to launch the new activity.
     * @param expectedSelectedTabUrl The URL of the tab that's expected to be selected.
     * @param expectedTabUrls The expected ordering of tab URLs after the merge.
     * @return The newly created ChromeTabbedActivity.
     */
    private ChromeTabbedActivity startNewChromeTabbedActivityAndAssert(Intent intent,
            String expectedSelectedTabUrl, String[] expectedTabUrls) {
        final ChromeTabbedActivity newActivity =
                (ChromeTabbedActivity) InstrumentationRegistry.getInstrumentation()
                        .startActivitySync(intent);

        // Wait for the tab state to be initialized.
        CriteriaHelper.pollUiThread(new Criteria("Tab state failed to initialize.") {
            @Override
            public boolean isSatisfied() {
                return newActivity.areTabModelsInitialized()
                        && newActivity.getTabModelSelector().isTabStateInitialized();
            }
        });

        assertTabModelMatchesExpectations(newActivity, expectedSelectedTabUrl, expectedTabUrls);

        return newActivity;
    }

    private void assertTabModelMatchesExpectations(final ChromeTabbedActivity activity,
            String expectedSelectedTabUrl, final String[] expectedTabUrls) {
        // Assert there are the correct number of tabs.
        Assert.assertEquals("Wrong number of normal tabs", expectedTabUrls.length,
                activity.getTabModelSelector().getModel(false).getCount());

        // Assert that the correct tab is selected.
        Assert.assertEquals("Wrong tab selected", expectedSelectedTabUrl,
                activity.getTabModelSelector().getCurrentTab().getUrl());

        // Assert that tabs are in the correct order.
        for (int i = 0; i < expectedTabUrls.length; i++) {
            Assert.assertEquals("Wrong tab at position " + i, expectedTabUrls[i],
                    activity.getTabModelSelector().getModel(false).getTabAt(i).getUrl());
        }
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergeIntoChromeTabbedActivity1() {
        mergeTabsAndAssert(mActivity1, mMergeIntoActivity1ExpectedTabs);
        mActivity1.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergeIntoChromeTabbedActivity2() {
        mergeTabsAndAssert(mActivity2, mMergeIntoActivity2ExpectedTabs);
        mActivity2.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergeOnColdStart() {
        String expectedSelectedUrl = mActivity1.getTabModelSelector().getCurrentTab().getUrl();

        // Create an intent to launch a new ChromeTabbedActivity.
        Intent intent = createChromeTabbedActivityIntent(mActivity1);

        // Save state.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity1.saveState();
            mActivity2.saveState();
        });

        // Destroy both activities.
        mActivity2.finishAndRemoveTask();
        CriteriaHelper.pollUiThread(new Criteria(
                "CTA2 should be destroyed, current state: " + mActivity2State) {
            @Override
            public boolean isSatisfied() {
                return mActivity2State == ActivityState.DESTROYED;
            }
        });

        mActivity1.finishAndRemoveTask();
        CriteriaHelper.pollUiThread(
                new Criteria("CTA should be destroyed, current state: " + mActivity1State) {
                    @Override
                    public boolean isSatisfied() {
                        return mActivity1State == ActivityState.DESTROYED;
                    }
                });

        // Start the new ChromeTabbedActivity.
        final ChromeTabbedActivity newActivity = startNewChromeTabbedActivityAndAssert(
                intent, expectedSelectedUrl, mMergeIntoActivity1ExpectedTabs);

        // Clean up.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergeOnColdStartFromChromeTabbedActivity2() throws Exception {
        String expectedSelectedUrl = mActivity2.getTabModelSelector().getCurrentTab().getUrl();

        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabModelSelectorImpl tabModelSelector =
                (TabModelSelectorImpl) mActivity2.getTabModelSelector();
        tabModelSelector.getTabPersistentStoreForTesting().addObserver(mockObserver);

        // Merge tabs into ChromeTabbedActivity2. Wait for the merge to finish, ensuring the
        // tab metadata file for ChromeTabbedActivity gets deleted before attempting to merge
        // on cold start.
        mergeTabsAndAssert(mActivity2, mMergeIntoActivity2ExpectedTabs);
        mockObserver.stateMergedCallback.waitForCallback(0, 1);

        // Create an intent to launch a new ChromeTabbedActivity.
        Intent intent = createChromeTabbedActivityIntent(mActivity2);

        // Destroy ChromeTabbedActivity2. ChromeTabbedActivity should have been destroyed during the
        // merge.
        mActivity2.finishAndRemoveTask();
        CriteriaHelper.pollUiThread(new Criteria("Both activities should be destroyed."
                + "CTA state: " + mActivity1State + " - CTA2State: " + mActivity2State) {
            @Override
            public boolean isSatisfied() {
                return mActivity1State == ActivityState.DESTROYED
                        && mActivity2State == ActivityState.DESTROYED;
            }
        });

        // Start the new ChromeTabbedActivity.
        final ChromeTabbedActivity newActivity = startNewChromeTabbedActivityAndAssert(
                intent, expectedSelectedUrl, mMergeIntoActivity2ExpectedTabs);

        // Clean up.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergeOnColdStartIntoChromeTabbedActivity2() throws TimeoutException {
        String CTA2ClassName = mActivity2.getClass().getName();
        String CTA2PackageName = mActivity2.getPackageName();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity1.saveState();
            mActivity2.saveState();
        });

        // Destroy both activities without removing tasks.
        mActivity1.finish();
        mActivity2.finish();

        CriteriaHelper.pollUiThread(new Criteria("Both activities should be destroyed."
                + "CTA state: " + mActivity1State + " - CTA2State: " + mActivity2State) {
            @Override
            public boolean isSatisfied() {
                return mActivity1State == ActivityState.DESTROYED
                        && mActivity2State == ActivityState.DESTROYED;
            }
        });

        // Send a main intent to restart ChromeTabbedActivity2.
        Intent CTA2MainIntent = new Intent(Intent.ACTION_MAIN);
        CTA2MainIntent.setClassName(CTA2PackageName, CTA2ClassName);
        InstrumentationRegistry.getInstrumentation().startActivitySync(CTA2MainIntent);

        mNewCTA2CallbackHelper.waitForCallback(0);

        CriteriaHelper.pollUiThread(new Criteria("CTA2 tab state failed to initialize.") {
            @Override
            public boolean isSatisfied() {
                return mNewCTA2.areTabModelsInitialized()
                        && mNewCTA2.getTabModelSelector().isTabStateInitialized();
            }
        });

        // Check that a merge occurred.
        Assert.assertEquals("Wrong number of tabs after restart.",
                mMergeIntoActivity2ExpectedTabs.length,
                mNewCTA2.getTabModelSelector().getModel(false).getCount());

        // TODO(twellington): When manually testing with "Don't keep activities" turned on in
        // developer settings, tabs are merged in the right order. In this test, however, the
        // order isn't quite as expected. Investigate replacing #finish() with something that
        // better simulates the activity being killed in the background due to OOM.

        // Clean up.
        mNewCTA2.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testMergeWhileInTabSwitcher() {
        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                mActivity1.getLayoutManager(), true, false);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity1.getLayoutManager().showOverview(false); });
        overviewModeWatcher.waitForBehavior();

        mergeTabsAndAssert(mActivity1, mMergeIntoActivity1ExpectedTabs);
        Assert.assertTrue("Overview mode should still be showing", mActivity1.isInOverviewMode());
        mActivity1.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergeWithNoTabs() {
        // Close all tabs and wait for the callback.
        ChromeTabUtils.closeAllTabs(InstrumentationRegistry.getInstrumentation(), mActivity1);

        String[] expectedTabUrls = new String[3];
        for (int i = 0; i < 3; i++) {
            expectedTabUrls[i] = mMergeIntoActivity2ExpectedTabs[i];
        }

        // The first tab should be selected after the merge.
        mergeTabsAndAssert(mActivity1, expectedTabUrls, 3, expectedTabUrls[0]);
        mActivity1.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergingIncognitoTabs() {
        // Incognito tabs must be fully loaded so that their tab states are written out.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(), mActivity1, TEST_URL_5, true);
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(), mActivity2, TEST_URL_6, true);

        // Save state.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity1.saveState();
            mActivity2.saveState();
        });

        Assert.assertEquals("Wrong number of incognito tabs in ChromeTabbedActivity", 1,
                mActivity1.getTabModelSelector().getModel(true).getCount());
        Assert.assertEquals("Wrong number of tabs in ChromeTabbedActivity", 5,
                mActivity1.getTabModelSelector().getTotalTabCount());
        Assert.assertEquals("Wrong number of incognito tabs in ChromeTabbedActivity2", 1,
                mActivity2.getTabModelSelector().getModel(true).getCount());
        Assert.assertEquals("Wrong number of tabs in ChromeTabbedActivity2", 4,
                mActivity2.getTabModelSelector().getTotalTabCount());

        String selectedUrl = mActivity1.getTabModelSelector().getCurrentTab().getUrl();
        mergeTabsAndAssert(mActivity1, mMergeIntoActivity1ExpectedTabs, 9, selectedUrl);
    }
}
