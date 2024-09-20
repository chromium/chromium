// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build.VERSION_CODES;
import android.provider.Browser;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import com.google.common.collect.Lists;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** Instrumentation tests for ChromeTabbedActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ChromeTabbedActivityTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Mock private AndroidPermissionDelegate mPermissionDelegate;
    @Mock private TabGroupSyncService mTabGroupSyncService;

    private static final String FILE_PATH = "/chrome/test/data/android/test.html";

    private static final String TABBED_SESSION_CONTAINED_GOOGLE_SEARCH_HISTOGRAM =
            "Session.Android.TabbedSessionContainedGoogleSearch";

    private ChromeTabbedActivity mActivity;

    private UmaSessionStats mUmaSessionStats;

    @Before
    public void setUp() {
        mActivity = sActivityTestRule.getActivity();

        Context appContext =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUmaSessionStats = new UmaSessionStats(appContext);
                });
    }

    /**
     * Verifies that the front tab receives the hide() call when the activity is stopped (hidden);
     * and that it receives the show() call when the activity is started again. This is a regression
     * test for http://crbug.com/319804 .
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1347506")
    public void testTabVisibility() {
        // Create two tabs - tab[0] in the foreground and tab[1] in the background.
        final Tab[] tabs = new Tab[2];
        sActivityTestRule.getTestServer(); // Triggers the lazy initialization of the test server.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Foreground tab.
                    ChromeTabCreator tabCreator = mActivity.getCurrentTabCreator();
                    tabs[0] =
                            tabCreator.createNewTab(
                                    new LoadUrlParams(
                                            sActivityTestRule.getTestServer().getURL(FILE_PATH)),
                                    TabLaunchType.FROM_CHROME_UI,
                                    null);
                    // Background tab.
                    tabs[1] =
                            tabCreator.createNewTab(
                                    new LoadUrlParams(
                                            sActivityTestRule.getTestServer().getURL(FILE_PATH)),
                                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                    null);
                });

        // Verify that the front tab is in the 'visible' state.
        Assert.assertFalse(tabs[0].isHidden());
        Assert.assertTrue(tabs[1].isHidden());

        // Fake sending the activity to background.
        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onPause());
        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onStop());
        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onWindowFocusChanged(false));
        // Verify that both Tabs are hidden.
        Assert.assertTrue(tabs[0].isHidden());
        Assert.assertTrue(tabs[1].isHidden());

        // Fake bringing the activity back to foreground.
        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onWindowFocusChanged(true));
        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onStart());
        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onResume());
        // Verify that the front tab is in the 'visible' state.
        Assert.assertFalse(tabs[0].isHidden());
        Assert.assertTrue(tabs[1].isHidden());
    }

    @Test
    @SmallTest
    public void testTabAnimationsCorrectlyEnabled() {
        boolean animationsEnabled =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivity.getLayoutManager().animationsEnabled());
        Assert.assertEquals(animationsEnabled, DeviceClassManager.enableAnimations());
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testTabModelSelectorObserverOnTabStateInitialized() {
        // Get the original value of |mCreatedTabOnStartup|.
        boolean createdTabOnStartup = mActivity.getCreatedTabOnStartupForTesting();

        // Reset the values of |mCreatedTabOnStartup| and |MultiInstanceManager.mTabModelObserver|.
        // This tab model selector observer should be registered in MultiInstanceManager on tab
        // state initialization irrespective of the value of |mCreatedTabOnStartup|.
        mActivity.setCreatedTabOnStartupForTesting(false);
        mActivity.getMultiInstanceMangerForTesting().setTabModelObserverForTesting(null);

        var tabModelSelectorObserver = mActivity.getTabModelSelectorObserverForTesting();
        ThreadUtils.runOnUiThreadBlocking(tabModelSelectorObserver::onTabStateInitialized);
        Assert.assertTrue(
                "Regular tab count should be written to SharedPreferences after tab state"
                        + " initialization.",
                ChromeSharedPreferences.getInstance()
                                .readIntsWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TAB_COUNT)
                                .size()
                        > 0);
        Assert.assertTrue(
                "Incognito tab count should be written to SharedPreferences after tab state"
                        + " initialization.",
                ChromeSharedPreferences.getInstance()
                                .readIntsWithPrefix(
                                        ChromePreferenceKeys.MULTI_INSTANCE_INCOGNITO_TAB_COUNT)
                                .size()
                        > 0);

        // Restore the original value of |mCreatedTabOnStartup|.
        mActivity.setCreatedTabOnStartupForTesting(createdTabOnStartup);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1347506")
    public void testMultiUrlIntent() {
        Intent viewIntent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(sActivityTestRule.getTestServer().getURL("/first")));
        viewIntent.putExtra(
                Browser.EXTRA_APPLICATION_ID, mActivity.getApplicationContext().getPackageName());
        viewIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        viewIntent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, PageTransition.AUTO_BOOKMARK);
        viewIntent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        viewIntent.setClass(mActivity, ChromeLauncherActivity.class);
        ArrayList<String> extraUrls =
                Lists.newArrayList(
                        sActivityTestRule.getTestServer().getURL("/second"),
                        sActivityTestRule.getTestServer().getURL("/third"));
        viewIntent.putExtra(IntentHandler.EXTRA_ADDITIONAL_URLS, extraUrls);
        IntentUtils.addTrustedIntentExtras(viewIntent);

        mActivity.getApplicationContext().startActivity(viewIntent);
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(4));
                    Criteria.checkThat(
                            tabModel.getTabAt(1).getUrl().getSpec(), Matchers.endsWith("first"));
                    Criteria.checkThat(
                            tabModel.getTabAt(2).getUrl().getSpec(), Matchers.endsWith("second"));
                    Criteria.checkThat(
                            tabModel.getTabAt(3).getUrl().getSpec(), Matchers.endsWith("third"));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivity
                                .getCurrentTabModel()
                                .closeTabs(TabClosureParams.closeAllTabs().build()));

        viewIntent.putExtra(IntentHandler.EXTRA_OPEN_ADDITIONAL_URLS_IN_TAB_GROUP, true);
        mActivity.getApplicationContext().startActivity(viewIntent);
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(3));
                    Criteria.checkThat(
                            tabModel.getTabAt(0).getUrl().getSpec(), Matchers.endsWith("first"));
                    int parentId = tabModel.getTabAt(0).getId();
                    Criteria.checkThat(
                            tabModel.getTabAt(1).getUrl().getSpec(), Matchers.endsWith("second"));
                    Criteria.checkThat(tabModel.getTabAt(1).getParentId(), Matchers.is(parentId));
                    Criteria.checkThat(
                            tabModel.getTabAt(2).getUrl().getSpec(), Matchers.endsWith("third"));
                    Criteria.checkThat(tabModel.getTabAt(2).getParentId(), Matchers.is(parentId));
                });

        viewIntent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        mActivity.getApplicationContext().startActivity(viewIntent);
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.isIncognito(), Matchers.is(true));
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(3));
                    Criteria.checkThat(
                            tabModel.getTabAt(0).getUrl().getSpec(), Matchers.endsWith("first"));
                    Criteria.checkThat(
                            tabModel.getTabAt(1).getUrl().getSpec(), Matchers.endsWith("second"));
                    Criteria.checkThat(
                            tabModel.getTabAt(2).getUrl().getSpec(), Matchers.endsWith("third"));
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.REDIRECT_EXPLICIT_CTA_INTENTS_TO_EXISTING_ACTIVITY)
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testExplicitViewIntent_OpensInExistingLiveActivity() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                ChromeTabbedActivity
                                        .HISTOGRAM_EXPLICIT_VIEW_INTENT_FINISHED_NEW_ACTIVITY,
                                true,
                                1)
                        .build();
        int initialWindowCount = MultiWindowUtils.getInstanceCount();
        Intent intent =
                new Intent(Intent.ACTION_VIEW, Uri.parse(JUnitTestGURLs.EXAMPLE_URL.getSpec()));
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.setClass(mActivity, ChromeTabbedActivity.class);

        // The newly created ChromeTabbedActivity (created via #startActivity()) should be
        // destroyed, and the intent should be launched in the existing ChromeTabbedActivity.
        ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class,
                Stage.DESTROYED,
                () -> mActivity.getApplicationContext().startActivity(intent));

        Assert.assertEquals(
                "No new window should be opened.",
                initialWindowCount,
                MultiWindowUtils.getInstanceCount());
        // A new tab should be opened in the existing ChromeTabbedActivity.
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(2));
                });
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_FINISHING)
    public void testHandleMismatchedIndices_ActivityFinishing() throws ExecutionException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(
                                ChromeTabbedActivity
                                        .HISTOGRAM_MISMATCHED_INDICES_ACTIVITY_CREATION_TIME_DELTA,
                                1)
                        .build();
        // Create two new ChromeTabbedActivity's.
        ChromeTabbedActivity activity1 = createActivityForMismatchedIndicesTest();
        ChromeTabbedActivity activity2 = createActivityForMismatchedIndicesTest();

        // Assume that activity1 is going to finish().
        activity1.finish();

        // Trigger mismatched indices handling, this should destroy activity1's tab persistent store
        // instance.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        activity2.handleMismatchedIndices(
                                activity1,
                                /* isActivityInAppTasks= */ true,
                                /* isActivityInSameTask= */ false));
        Assert.assertTrue(
                "Boolean |mTabPersistentStoreDestroyedEarly| should be true.",
                activity1
                        .getTabModelOrchestratorSupplier()
                        .get()
                        .getTabPersistentStoreDestroyedEarlyForTesting());
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_IN_SAME_TASK)
    public void testHandleMismatchedIndices_ActivityInSameTask() throws ExecutionException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(
                                ChromeTabbedActivity
                                        .HISTOGRAM_MISMATCHED_INDICES_ACTIVITY_CREATION_TIME_DELTA,
                                1)
                        .build();

        // Create two new ChromeTabbedActivity's.
        ChromeTabbedActivity activity1 = createActivityForMismatchedIndicesTest();
        ChromeTabbedActivity activity2 = createActivityForMismatchedIndicesTest();

        // Trigger mismatched indices handling assuming that activity1 and activity2 are in the same
        // task, this should destroy activity1's tab persistent store instance.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        activity2.handleMismatchedIndices(
                                activity1,
                                /* isActivityInAppTasks= */ true,
                                /* isActivityInSameTask= */ true));
        Assert.assertTrue(
                "Boolean |mTabPersistentStoreDestroyedEarly| should be true.",
                activity1
                        .getTabModelOrchestratorSupplier()
                        .get()
                        .getTabPersistentStoreDestroyedEarlyForTesting());

        // activity1 should be subsequently destroyed.
        ApplicationTestUtils.waitForActivityState(activity1, Stage.DESTROYED);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    @EnableFeatures(
            ChromeFeatureList.TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_NOT_IN_APP_TASKS)
    public void testHandleMismatchedIndices_ActivityNotInAppTasks() throws ExecutionException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(
                                ChromeTabbedActivity
                                        .HISTOGRAM_MISMATCHED_INDICES_ACTIVITY_CREATION_TIME_DELTA,
                                1)
                        .build();

        // Create two new ChromeTabbedActivity's.
        ChromeTabbedActivity activity1 = createActivityForMismatchedIndicesTest();
        ChromeTabbedActivity activity2 = createActivityForMismatchedIndicesTest();

        // Trigger mismatched indices handling assuming that activity1 is not in AppTasks, this
        // should destroy activity1's tab persistent store instance.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        activity2.handleMismatchedIndices(
                                activity1,
                                /* isActivityInAppTasks= */ false,
                                /* isActivityInSameTask= */ false));
        Assert.assertTrue(
                "Boolean |mTabPersistentStoreDestroyedEarly| should be true.",
                activity1
                        .getTabModelOrchestratorSupplier()
                        .get()
                        .getTabPersistentStoreDestroyedEarlyForTesting());

        // activity1 should be subsequently destroyed.
        ApplicationTestUtils.waitForActivityState(activity1, Stage.DESTROYED);

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testSessionContainedGoogleSearchPage() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(TABBED_SESSION_CONTAINED_GOOGLE_SEARCH_HISTOGRAM, true)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUmaSessionStats.startNewSession(
                            ActivityType.TABBED, null, mPermissionDelegate);
                    mActivity.onResumeWithNative();
                });
        // Load Google SRP twice, but ensure histogram is only recorded to once.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                JUnitTestGURLs.SEARCH_URL.getSpec(),
                false);

        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                JUnitTestGURLs.SEARCH_URL.getSpec(),
                false);
        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onPauseWithNative());

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testSessionDidNotContainGoogleSearchPage() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                TABBED_SESSION_CONTAINED_GOOGLE_SEARCH_HISTOGRAM, false)
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUmaSessionStats.startNewSession(
                            ActivityType.TABBED, null, mPermissionDelegate);
                    mActivity.onResumeWithNative();
                });

        // Histogram record is false when session does not contain SRP.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                mActivity,
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                false);
        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onPauseWithNative());

        histogramWatcher.assertExpected();
    }

    private ChromeTabbedActivity createActivityForMismatchedIndicesTest() {
        // Launch a new ChromeTabbedActivity intent with the FLAG_ACTIVITY_MULTIPLE_TASK set to
        // ensure that a new activity is created. Note that generally our logs indicate
        // FLAG_ACTIVITY_MULTIPLE_TASK is not set on incoming intents, however, this is generally
        // the only way to get new ChromeTabbedActivity's in a non-error case.
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.setClass(mActivity, ChromeTabbedActivity.class);

        return ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class,
                Stage.CREATED,
                () -> mActivity.getApplicationContext().startActivity(intent));
    }

    @Test
    @MediumTest
    // Intentionally not batched due to recreating activity.
    @RequiresRestart
    @DisabledTest(message = "crbug.com/1187320 This doesn't work with FeedV2 and crbug.com/1096295")
    public void testActivityCanBeGarbageCollectedAfterFinished() {
        WeakReference<ChromeTabbedActivity> activityRef =
                new WeakReference<>(sActivityTestRule.getActivity());

        ChromeTabbedActivity activity =
                ApplicationTestUtils.recreateActivity(sActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        sActivityTestRule.setActivity(activity);

        CriteriaHelper.pollUiThread(
                () -> GarbageCollectionTestUtils.canBeGarbageCollected(activityRef));
    }

    @Test
    @MediumTest
    public void testBackShouldCloseTab() {
        sActivityTestRule.getTestServer(); // Triggers the lazy initialization of the test server.
        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            ChromeTabCreator tabCreator = mActivity.getCurrentTabCreator();
                            return tabCreator.createNewTab(
                                    new LoadUrlParams(
                                            sActivityTestRule.getTestServer().getURL(FILE_PATH)),
                                    TabLaunchType.FROM_LINK,
                                    null);
                        });
        boolean ret =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity.backShouldCloseTab(tab);
                        });
        Assert.assertTrue(ret);
    }

    @Test
    @MediumTest
    public void testBackShouldCloseTab_Collaboration() {

        sActivityTestRule.getTestServer(); // Triggers the lazy initialization of the test server.
        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            ChromeTabCreator tabCreator = mActivity.getCurrentTabCreator();
                            Tab newTab =
                                    tabCreator.createNewTab(
                                            new LoadUrlParams(
                                                    sActivityTestRule
                                                            .getTestServer()
                                                            .getURL(FILE_PATH)),
                                            TabLaunchType.FROM_LINK,
                                            null);
                            TabGroupModelFilter filter =
                                    (TabGroupModelFilter)
                                            mActivity
                                                    .getTabModelSelector()
                                                    .getTabModelFilterProvider()
                                                    .getTabModelFilter(false);
                            filter.createSingleTabGroup(newTab, false);
                            return newTab;
                        });

        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        savedTab.localId = tab.getId();

        String syncId = "sync_id";
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.syncId = syncId;
        savedTabGroup.localId = new LocalTabGroupId(tab.getTabGroupId());
        savedTabGroup.collaborationId = "collaboration_id";
        savedTabGroup.savedTabs = List.of(savedTab);

        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedTabGroup);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {syncId});

        boolean ret =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity.backShouldCloseTab(tab);
                        });
        Assert.assertFalse(ret);
    }
}
