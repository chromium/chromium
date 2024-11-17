// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreTest.MockTabPersistentStoreObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Collections;
import java.util.concurrent.TimeoutException;

/** Tests merging tab models for Android N+ multi-instance. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableIf.Build(sdk_is_greater_than = VERSION_CODES.S_V2) // https://crbug.com/1297370
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
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.writeBoolean(ChromePreferenceKeys.TABMODEL_HAS_RUN_FILE_MIGRATION, true);
        prefs.writeBoolean(
                ChromePreferenceKeys.TABMODEL_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION, true);

        // Some of the logic for when to trigger a merge depends on whether the activity is in
        // multi-window mode. Set isInMultiWindowMode to true to avoid merging unexpectedly.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);

        // Initialize activities.
        mActivity1 = mActivityTestRule.getActivity();
        // Start multi-instance mode so that ChromeTabbedActivity's check for whether the activity
        // is started up correctly doesn't fail.
        MultiInstanceManager.onMultiInstanceModeStarted();
        mActivity2 =
                MultiWindowTestHelper.createSecondChromeTabbedActivity(
                        mActivity1, new LoadUrlParams(TEST_URL_7));
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivity2.areTabModelsInitialized(), Matchers.is(true));
                    Criteria.checkThat(
                            mActivity2.getTabModelSelector().isTabStateInitialized(),
                            Matchers.is(true));
                });

        // Create a few tabs in each activity.
        createTabsOnUiThread();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Initialize activity states and register for state change events.
                    mActivity1State = ApplicationStatus.getStateForActivity(mActivity1);
                    mActivity2State = ApplicationStatus.getStateForActivity(mActivity2);
                    ApplicationStatus.registerStateListenerForAllActivities(
                            new ActivityStateListener() {
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
                });
    }

    /**
     * Creates new tabs in both ChromeTabbedActivity and ChromeTabbedActivity2 and asserts that each
     * has the expected number of tabs.
     */
    private void createTabsOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Create normal tabs.
                    mActivity1
                            .getTabCreator(false)
                            .createNewTab(
                                    new LoadUrlParams(TEST_URL_0),
                                    TabLaunchType.FROM_CHROME_UI,
                                    null);
                    mActivity1
                            .getTabCreator(false)
                            .createNewTab(
                                    new LoadUrlParams(TEST_URL_1),
                                    TabLaunchType.FROM_CHROME_UI,
                                    null);
                    mActivity1
                            .getTabCreator(false)
                            .createNewTab(
                                    new LoadUrlParams(TEST_URL_2),
                                    TabLaunchType.FROM_CHROME_UI,
                                    null);
                    mActivity2
                            .getTabCreator(false)
                            .createNewTab(
                                    new LoadUrlParams(TEST_URL_3),
                                    TabLaunchType.FROM_CHROME_UI,
                                    null);
                    mActivity2
                            .getTabCreator(false)
                            .createNewTab(
                                    new LoadUrlParams(TEST_URL_4),
                                    TabLaunchType.FROM_CHROME_UI,
                                    null);

                    mActivity1.saveState();
                    mActivity2.saveState();
                });

        // ChromeTabbedActivity should have four normal tabs, the one it started with and the three
        // just created.
        Assert.assertEquals(
                "Wrong number of tabs in ChromeTabbedActivity",
                4,
                mActivity1.getTabModelSelector().getModel(false).getCount());

        // ChromeTabbedActivity2 should have three normal tabs, the one it started with and the two
        // just created.
        Assert.assertEquals(
                "Wrong number of tabs in ChromeTabbedActivity2",
                3,
                mActivity2.getTabModelSelector().getModel(false).getCount());

        // Construct expected tabs.
        mMergeIntoActivity1ExpectedTabs = new String[7];
        mMergeIntoActivity2ExpectedTabs = new String[7];
        for (int i = 0; i < 4; i++) {
            mMergeIntoActivity1ExpectedTabs[i] =
                    ChromeTabUtils.getUrlStringOnUiThread(
                            mActivity1.getTabModelSelector().getModel(false).getTabAt(i));
            mMergeIntoActivity2ExpectedTabs[i + 3] =
                    ChromeTabUtils.getUrlStringOnUiThread(
                            mActivity1.getTabModelSelector().getModel(false).getTabAt(i));
        }
        for (int i = 0; i < 3; i++) {
            mMergeIntoActivity2ExpectedTabs[i] =
                    ChromeTabUtils.getUrlStringOnUiThread(
                            mActivity2.getTabModelSelector().getModel(false).getTabAt(i));
            mMergeIntoActivity1ExpectedTabs[i + 4] =
                    ChromeTabUtils.getUrlStringOnUiThread(
                            mActivity2.getTabModelSelector().getModel(false).getTabAt(i));
        }
    }

    private void mergeTabsAndAssert(
            final ChromeTabbedActivity activity, final String[] expectedTabUrls) {
        String selectedTabUrl =
                ChromeTabUtils.getUrlStringOnUiThread(
                        activity.getTabModelSelector().getCurrentTab());
        mergeTabsAndAssert(activity, expectedTabUrls, expectedTabUrls.length, selectedTabUrl);
    }

    /**
     * Merges tabs into the provided activity and asserts that the tab model looks as expected.
     *
     * @param activity The activity to merge into.
     * @param expectedTabUrls The expected ordering of normal tab URLs after the merge.
     * @param expectedNumberOfTabs The expected number of tabs after the merge.
     * @param expectedSelectedTabUrl The expected URL of the selected tab after the merge.
     */
    private void mergeTabsAndAssert(
            final ChromeTabbedActivity activity,
            final String[] expectedTabUrls,
            final int expectedNumberOfTabs,
            String expectedSelectedTabUrl) {
        // Merge tabs into the activity.
        ThreadUtils.runOnUiThreadBlocking(
                () -> activity.getMultiInstanceMangerForTesting().maybeMergeTabs());

        // Wait for all tabs to be merged into the activity.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Total tab count incorrect.",
                            activity.getTabModelSelector().getTotalTabCount(),
                            Matchers.is(expectedNumberOfTabs));
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
     *
     * @param intent The intent to launch the new activity.
     * @param expectedSelectedTabUrl The URL of the tab that's expected to be selected.
     * @param expectedTabUrls The expected ordering of tab URLs after the merge.
     * @return The newly created ChromeTabbedActivity.
     */
    private ChromeTabbedActivity startNewChromeTabbedActivityAndAssert(
            Intent intent, String expectedSelectedTabUrl, String[] expectedTabUrls) {
        final ChromeTabbedActivity newActivity =
                (ChromeTabbedActivity)
                        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        // Wait for the tab state to be initialized.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(newActivity.areTabModelsInitialized(), Matchers.is(true));
                    Criteria.checkThat(
                            newActivity.getTabModelSelector().isTabStateInitialized(),
                            Matchers.is(true));
                });

        assertTabModelMatchesExpectations(newActivity, expectedSelectedTabUrl, expectedTabUrls);

        return newActivity;
    }

    private void assertTabModelMatchesExpectations(
            final ChromeTabbedActivity activity,
            String expectedSelectedTabUrl,
            final String[] expectedTabUrls) {
        // Assert there are the correct number of tabs.
        Assert.assertEquals(
                "Wrong number of normal tabs",
                expectedTabUrls.length,
                activity.getTabModelSelector().getModel(false).getCount());

        // Assert that the correct tab is selected.
        Assert.assertEquals(
                "Wrong tab selected",
                expectedSelectedTabUrl,
                ChromeTabUtils.getUrlStringOnUiThread(
                        activity.getTabModelSelector().getCurrentTab()));

        // Assert that tabs are in the correct order.
        for (int i = 0; i < expectedTabUrls.length; i++) {
            Assert.assertEquals(
                    "Wrong tab at position " + i,
                    expectedTabUrls[i],
                    ChromeTabUtils.getUrlStringOnUiThread(
                            activity.getTabModelSelector().getModel(false).getTabAt(i)));
        }
    }

    /**
     * Wait until the activity is on an expected or not expected state.
     *
     * @param state The expected state or not one.
     * @param activity The activity whose state will be observed.
     * @param expected If true, wait until activity is on the {@code state}; otherwise, wait util
     *     activity is on any state other than {@code state}.
     */
    private void waitForActivityStateChange(
            @ActivityState int state, Activity activity, boolean expected) throws TimeoutException {
        // do nothing if already in expected state.
        CallbackHelper helper = new CallbackHelper();
        ApplicationStatus.ActivityStateListener listener =
                (act, newState) -> {
                    if (expected == (state == newState)) helper.notifyCalled();
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int currentState = ApplicationStatus.getStateForActivity(activity);
                    if (expected == (state == currentState)) {
                        helper.notifyCalled();
                        return;
                    }
                    ApplicationStatus.registerStateListenerForActivity(listener, activity);
                });
        helper.waitForOnly();
        // listener was registered on UiThread. So it should be unregistered on UiThread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.unregisterActivityStateListener(listener);
                });
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    @DisabledTest(message = "https://crbug.com/1275082")
    public void testMergeIntoChromeTabbedActivity1() {
        mergeTabsAndAssert(mActivity1, mMergeIntoActivity1ExpectedTabs);
        mActivity1.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    @DisabledTest(message = "https://crbug.com/1275082")
    public void testMergeIntoChromeTabbedActivity2() {
        mergeTabsAndAssert(mActivity2, mMergeIntoActivity2ExpectedTabs);
        mActivity2.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    @DisabledTest(message = "https://crbug.com/1275082")
    public void testMergeOnColdStart() {
        String expectedSelectedUrl =
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivity1.getTabModelSelector().getCurrentTab());

        // Create an intent to launch a new ChromeTabbedActivity.
        Intent intent = createChromeTabbedActivityIntent(mActivity1);

        // Save state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity1.saveState();
                    mActivity2.saveState();
                });

        // Destroy both activities.
        mActivity2.finishAndRemoveTask();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "CTA2 should be destroyed",
                            mActivity2State,
                            Matchers.is(ActivityState.DESTROYED));
                });

        mActivity1.finishAndRemoveTask();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "CTA should be destroyed",
                            mActivity1State,
                            Matchers.is(ActivityState.DESTROYED));
                });

        // Start the new ChromeTabbedActivity.
        final ChromeTabbedActivity newActivity =
                startNewChromeTabbedActivityAndAssert(
                        intent, expectedSelectedUrl, mMergeIntoActivity1ExpectedTabs);

        // Clean up.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    @DisabledTest(message = "https://crbug.com/1275082")
    public void testMergeOnColdStartFromChromeTabbedActivity2() throws Exception {
        String expectedSelectedUrl =
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivity2.getTabModelSelector().getCurrentTab());

        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabModelSelectorImpl tabModelSelector =
                (TabModelSelectorImpl) mActivity2.getTabModelSelector();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity2
                            .getTabModelOrchestratorSupplier()
                            .get()
                            .getTabPersistentStore()
                            .addObserver(mockObserver);
                });

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
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "CTA should be destroyed",
                            mActivity1State,
                            Matchers.is(ActivityState.DESTROYED));
                    Criteria.checkThat(
                            "CTA2 should be destroyed",
                            mActivity2State,
                            Matchers.is(ActivityState.DESTROYED));
                });

        // Start the new ChromeTabbedActivity.
        final ChromeTabbedActivity newActivity =
                startNewChromeTabbedActivityAndAssert(
                        intent, expectedSelectedUrl, mMergeIntoActivity2ExpectedTabs);

        // Clean up.
        newActivity.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    @DisabledTest(message = "https://crbug.com/1417018")
    public void testMergeOnColdStartIntoChromeTabbedActivity2() throws TimeoutException {
        String CTA2ClassName = mActivity2.getClass().getName();
        String CTA2PackageName = mActivity2.getPackageName();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity1.saveState();
                    mActivity2.saveState();
                });

        // Destroy both activities without removing tasks.
        mActivity1.finish();
        mActivity2.finish();

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "CTA should be destroyed",
                            mActivity1State,
                            Matchers.is(ActivityState.DESTROYED));
                    Criteria.checkThat(
                            "CTA2 should be destroyed",
                            mActivity2State,
                            Matchers.is(ActivityState.DESTROYED));
                });

        // Send a main intent to restart ChromeTabbedActivity2.
        Intent CTA2MainIntent = new Intent(Intent.ACTION_MAIN);
        CTA2MainIntent.setClassName(CTA2PackageName, CTA2ClassName);
        CTA2MainIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        InstrumentationRegistry.getInstrumentation().startActivitySync(CTA2MainIntent);

        mNewCTA2CallbackHelper.waitForCallback(0);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mNewCTA2.areTabModelsInitialized(), Matchers.is(true));
                    Criteria.checkThat(
                            mNewCTA2.getTabModelSelector().isTabStateInitialized(),
                            Matchers.is(true));
                });

        // Check that a merge occurred.
        Assert.assertEquals(
                "Wrong number of tabs after restart.",
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
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @DisabledTest(message = "https://crbug.com/1275082")
    public void testMergeWhileInTabSwitcher() {
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivity1.getLayoutManager(), LayoutType.TAB_SWITCHER, false);

        mergeTabsAndAssert(mActivity1, mMergeIntoActivity1ExpectedTabs);
        Assert.assertTrue("Overview mode should still be showing", mActivity1.isInOverviewMode());
        mActivity1.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    @DisabledTest(message = "https://crbug.com/1275082")
    public void testMergeWithNoTabs() {
        // Enter the tab switcher before closing all tabs with grid tab switcher enabled, otherwise
        // the activity is killed and the test fails.
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivity1.getLayoutManager(), LayoutType.TAB_SWITCHER, false);

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
    @DisabledTest(message = "https://crbug.com/1190012")
    public void testMergingIncognitoTabs() {
        // Incognito tabs must be fully loaded so that their tab states are written out.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(), mActivity1, TEST_URL_5, true);
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(), mActivity2, TEST_URL_6, true);

        // Save state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity1.saveState();
                    mActivity2.saveState();
                });

        Assert.assertEquals(
                "Wrong number of incognito tabs in ChromeTabbedActivity",
                1,
                mActivity1.getTabModelSelector().getModel(true).getCount());
        Assert.assertEquals(
                "Wrong number of tabs in ChromeTabbedActivity",
                5,
                mActivity1.getTabModelSelector().getTotalTabCount());
        Assert.assertEquals(
                "Wrong number of incognito tabs in ChromeTabbedActivity2",
                1,
                mActivity2.getTabModelSelector().getModel(true).getCount());
        Assert.assertEquals(
                "Wrong number of tabs in ChromeTabbedActivity2",
                4,
                mActivity2.getTabModelSelector().getTotalTabCount());

        String selectedUrl =
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivity1.getTabModelSelector().getCurrentTab());
        mergeTabsAndAssert(mActivity1, mMergeIntoActivity1ExpectedTabs, 9, selectedUrl);
    }

    @Test
    @LargeTest
    @DisableIf.Build(sdk_is_less_than = VERSION_CODES.P)
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338997261
    public void testMergeOnMultiDisplay_CTA_Resumed_CTA2_Not_Resumed() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity1.saveState();
                    mActivity2.saveState();
                });
        MultiInstanceManager m1 = mActivity1.getMultiInstanceMangerForTesting();
        MultiInstanceManager m2 = mActivity2.getMultiInstanceMangerForTesting();

        // Ensure Activity 1 is resumed on the front.
        Intent intent = new Intent(mActivity1, mActivity1.getClass());
        intent.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        mActivity1.startActivity(intent);
        waitForActivityStateChange(ActivityState.RESUMED, mActivity2, false);
        waitForActivityStateChange(ActivityState.RESUMED, mActivity1, true);

        MultiInstanceManager.setTestDisplayIds(Collections.singletonList(0));
        m1.setCurrentDisplayIdForTesting(0);
        m2.setCurrentDisplayIdForTesting(1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    m1.getDisplayListenerForTesting().onDisplayRemoved(1);
                    m2.getDisplayListenerForTesting().onDisplayRemoved(1);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Total tab count incorrect.",
                            mActivity1.getTabModelSelector().getTotalTabCount(),
                            Matchers.is(7));
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "CTA should not be destroyed",
                            mActivity1State,
                            Matchers.not(ActivityState.DESTROYED));
                    Criteria.checkThat(
                            "CTA2 should be destroyed",
                            mActivity2State,
                            Matchers.is(ActivityState.DESTROYED));
                });
        mActivity1.finishAndRemoveTask();
        mActivity2.finishAndRemoveTask();
    }

    @Test
    @LargeTest
    @DisableIf.Build(sdk_is_less_than = VERSION_CODES.P)
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338997261
    public void testMergeOnMultiDisplay_OnDisplayChanged() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity1.saveState();
                    mActivity2.saveState();
                });
        MultiInstanceManager m1 = mActivity1.getMultiInstanceMangerForTesting();
        MultiInstanceManager m2 = mActivity2.getMultiInstanceMangerForTesting();

        // Ensure Activity 1 is resumed on the front.
        Intent intent = new Intent(mActivity1, mActivity1.getClass());
        intent.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        mActivity1.startActivity(intent);
        waitForActivityStateChange(ActivityState.RESUMED, mActivity2, false);
        waitForActivityStateChange(ActivityState.RESUMED, mActivity1, true);

        MultiInstanceManager.setTestDisplayIds(Collections.singletonList(0));
        m1.setCurrentDisplayIdForTesting(0);
        m2.setCurrentDisplayIdForTesting(1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    m1.getDisplayListenerForTesting().onDisplayChanged(1);
                    m2.getDisplayListenerForTesting().onDisplayChanged(1);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Total tab count incorrect.",
                            mActivity1.getTabModelSelector().getTotalTabCount(),
                            Matchers.is(7));
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "CTA should not be destroyed",
                            mActivity1State,
                            Matchers.not(ActivityState.DESTROYED));
                    Criteria.checkThat(
                            "CTA2 should be destroyed",
                            mActivity2State,
                            Matchers.is(ActivityState.DESTROYED));
                });

        mActivity1.finishAndRemoveTask();
        mActivity2.finishAndRemoveTask();
    }
}
