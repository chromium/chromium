// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.TabbedMismatchedIndicesHandler.HISTOGRAM_MISMATCHED_INDICES_ACTIVITY_CREATION_TIME_DELTA;

import android.content.Intent;
import android.net.Uri;
import android.os.Build.VERSION_CODES;
import android.os.SystemClock;
import android.provider.Browser;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import com.google.common.collect.Lists;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoWindowNightModeStateProvider;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.MultiTabMetadata;
import org.chromium.chrome.browser.tabmodel.RedirectTabCreator;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.toolbar.top.tab_strip.StripVisibilityState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Instrumentation tests for ChromeTabbedActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Testing state in static singletons from multiple activities.")
public class ChromeTabbedActivityTest {
    private static final Token TAB_GROUP_ID = new Token(2L, 2L);
    private static final String TAB_GROUP_TITLE = "Regrouped tabs";
    private static final ArrayList<Map.Entry<Integer, String>> TAB_IDS_TO_URLS =
            new ArrayList<>(
                    List.of(
                            Map.entry(1, "https://www.amazon.com/"),
                            Map.entry(2, "https://www.youtube.com/"),
                            Map.entry(3, "https://www.facebook.com/")));
    private static final String FILE_PATH = "/chrome/test/data/android/test.html";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupSyncService mTabGroupSyncService;
    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        assertNotNull(mActivity);
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
        mActivityTestRule.getTestServer(); // Triggers the lazy initialization of the test server.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Foreground tab.
                    ChromeTabCreator tabCreator = mActivity.getCurrentTabCreator();
                    tabs[0] =
                            tabCreator.createNewTab(
                                    new LoadUrlParams(
                                            mActivityTestRule.getTestServer().getURL(FILE_PATH)),
                                    TabLaunchType.FROM_CHROME_UI,
                                    null);
                    // Background tab.
                    tabs[1] =
                            tabCreator.createNewTab(
                                    new LoadUrlParams(
                                            mActivityTestRule.getTestServer().getURL(FILE_PATH)),
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

    /** Verifies that the focused tab is IMPORTANT and unfocused tabs are MODERATE. */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.CHANGE_UNFOCUSED_PRIORITY})
    @DisableFeatures({ChromeFeatureList.PROCESS_RANK_POLICY_ANDROID})
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testTabImportance() {
        mActivityTestRule.getTestServer(); // Triggers the lazy initialization of the test server.
        final Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            ChromeTabCreator tabCreator = mActivity.getCurrentTabCreator();
                            return tabCreator.createNewTab(
                                    new LoadUrlParams(
                                            mActivityTestRule.getTestServer().getURL(FILE_PATH)),
                                    TabLaunchType.FROM_CHROME_UI,
                                    null);
                        });
        // Fake sending the activity to unfocused.
        @ChildProcessImportance
        int importance =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            mActivity.onTopResumedActivityChanged(false);
                            return TabTestUtils.getImportance(tab);
                        });
        // Verify that tab has importance MODERATE.
        Assert.assertEquals(
                "Tab process does not have importance MODERATE",
                ChildProcessImportance.MODERATE,
                importance);
        // Fake sending the activity to focused.
        importance =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            mActivity.onTopResumedActivityChanged(true);
                            return TabTestUtils.getImportance(tab);
                        });
        // Verify that tab has importance IMPORTANT.
        Assert.assertEquals(
                "Tab process does not have importance IMPORTANT",
                ChildProcessImportance.IMPORTANT,
                importance);
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
                        Uri.parse(mActivityTestRule.getTestServer().getURL("/first")));
        viewIntent.putExtra(
                Browser.EXTRA_APPLICATION_ID, mActivity.getApplicationContext().getPackageName());
        viewIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        viewIntent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, PageTransition.AUTO_BOOKMARK);
        viewIntent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        viewIntent.setClass(mActivity, ChromeLauncherActivity.class);
        ArrayList<String> extraUrls =
                Lists.newArrayList(
                        mActivityTestRule.getTestServer().getURL("/second"),
                        mActivityTestRule.getTestServer().getURL("/third"));
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
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeAllTabs().build(),
                                        /* allowDialog= */ false));

        viewIntent.putExtra(IntentHandler.EXTRA_OPEN_ADDITIONAL_URLS_IN_TAB_GROUP, true);
        mActivity.getApplicationContext().startActivity(viewIntent);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_GROUP_DRAG_DROP_ANDROID})
    public void testTabGroupIntent_collapseGroup() {
        testTabGroupIntent(/* shouldApplyCollapse= */ true);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_GROUP_DRAG_DROP_ANDROID})
    public void testTabGroupIntent_skipCollapseWhenStripHidden() {
        // Hide tab strip.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity
                            .getLayoutManager()
                            .getStripLayoutHelperManager()
                            .setStripVisibilityState(
                                    StripVisibilityState.HIDDEN_BY_FADE, /* clear= */ false);
                });

        // Collapse group should be skipped when strip is hidden.
        testTabGroupIntent(/* shouldApplyCollapse= */ false);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testExplicitViewIntent_OpensInExistingLiveActivity() {
        int initialWindowCount =
                MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY);
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
                MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY));
        // A new tab should be opened in the existing ChromeTabbedActivity.
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(2));
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testHandleMismatchedIndices_ActivityFinishing() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(
                                HISTOGRAM_MISMATCHED_INDICES_ACTIVITY_CREATION_TIME_DELTA, 1)
                        .build();
        // Create two new ChromeTabbedActivity's.
        ChromeTabbedActivity activity1 = createActivityForMismatchedIndicesTest();
        ChromeTabbedActivity activity2 = createActivityForMismatchedIndicesTest();
        MismatchedIndicesHandler handler2 = activity2.getMismatchedIndicesHandlerForTesting();

        // Assume that activity1 is going to finish().
        activity1.finish();

        // Trigger mismatched indices handling, this should destroy activity1's tab persistent store
        // instance.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        handler2.handleMismatchedIndices(
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
    public void testHandleMismatchedIndices_ActivityInSameTask() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(
                                HISTOGRAM_MISMATCHED_INDICES_ACTIVITY_CREATION_TIME_DELTA, 1)
                        .build();

        // Create two new ChromeTabbedActivity's.
        ChromeTabbedActivity activity1 = createActivityForMismatchedIndicesTest();
        ChromeTabbedActivity activity2 = createActivityForMismatchedIndicesTest();
        MismatchedIndicesHandler handler2 = activity2.getMismatchedIndicesHandlerForTesting();

        // Trigger mismatched indices handling assuming that activity1 and activity2 are in the same
        // task, this should destroy activity1's tab persistent store instance.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        handler2.handleMismatchedIndices(
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
    public void testHandleMismatchedIndices_ActivityNotInAppTasks() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(
                                HISTOGRAM_MISMATCHED_INDICES_ACTIVITY_CREATION_TIME_DELTA, 1)
                        .build();

        // Create two new ChromeTabbedActivity's.
        ChromeTabbedActivity activity1 = createActivityForMismatchedIndicesTest();
        ChromeTabbedActivity activity2 = createActivityForMismatchedIndicesTest();
        MismatchedIndicesHandler handler2 = activity2.getMismatchedIndicesHandlerForTesting();

        // Trigger mismatched indices handling assuming that activity1 is not in AppTasks, this
        // should destroy activity1's tab persistent store instance.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        handler2.handleMismatchedIndices(
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
    @MinAndroidSdkLevel(VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testNewRegularTab_SameWindow() {
        mActivityTestRule.getTestServer(); // Triggers the lazy initialization of the test server.
        ChromeTabCreator tabCreatorRegular = mActivity.getTabCreator(false);
        Assert.assertFalse(tabCreatorRegular instanceof RedirectTabCreator);

        LoadUrlParams param =
                new LoadUrlParams(mActivityTestRule.getTestServer().getURL(FILE_PATH));
        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return tabCreatorRegular.createNewTab(
                                    param, TabLaunchType.FROM_CHROME_UI, null);
                        });
        Assert.assertNotNull(tab);
        AtomicInteger regularTabCount = new AtomicInteger();
        AtomicInteger incognitoTabCount = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    regularTabCount.set(
                            mActivity
                                    .getTabModelSelector()
                                    .getModel(false)
                                    .getTabCountSupplier()
                                    .get());
                    incognitoTabCount.set(
                            mActivity
                                    .getTabModelSelector()
                                    .getModel(true)
                                    .getTabCountSupplier()
                                    .get());
                });
        Assert.assertEquals(2, regularTabCount.get());
        Assert.assertEquals(0, incognitoTabCount.get());
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testNewIncognitoTab_NewWindow() {
        mActivityTestRule.getTestServer(); // Triggers the lazy initialization of the test server.
        ChromeTabCreator tabCreatorIncognito = mActivity.getTabCreator(true);
        Assert.assertTrue(tabCreatorIncognito instanceof RedirectTabCreator);

        LoadUrlParams param =
                new LoadUrlParams(mActivityTestRule.getTestServer().getURL(FILE_PATH));
        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return tabCreatorIncognito.createNewTab(
                                    param, TabLaunchType.FROM_CHROME_UI, null);
                        });
        Assert.assertNull(tab);
        AtomicInteger regularTabCount = new AtomicInteger();
        AtomicInteger incognitoTabCount = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    regularTabCount.set(
                            mActivity
                                    .getTabModelSelector()
                                    .getModel(false)
                                    .getTabCountSupplier()
                                    .get());
                    incognitoTabCount.set(
                            mActivity
                                    .getTabModelSelector()
                                    .getModel(true)
                                    .getTabCountSupplier()
                                    .get());
                });
        Assert.assertEquals(1, regularTabCount.get());
        Assert.assertEquals(0, incognitoTabCount.get());
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testNewIncognitoTab_SameWindow() {
        mActivityTestRule.getTestServer(); // Triggers the lazy initialization of the test server.
        ChromeTabCreator tabCreatorIncognito = mActivity.getTabCreator(true);
        Assert.assertFalse(tabCreatorIncognito instanceof RedirectTabCreator);

        LoadUrlParams param =
                new LoadUrlParams(mActivityTestRule.getTestServer().getURL(FILE_PATH));
        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return tabCreatorIncognito.createNewTab(
                                    param, TabLaunchType.FROM_CHROME_UI, null);
                        });
        Assert.assertNotNull(tab);
        AtomicInteger regularTabCount = new AtomicInteger();
        AtomicInteger incognitoTabCount = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    regularTabCount.set(
                            mActivity
                                    .getTabModelSelector()
                                    .getModel(false)
                                    .getTabCountSupplier()
                                    .get());
                    incognitoTabCount.set(
                            mActivity
                                    .getTabModelSelector()
                                    .getModel(true)
                                    .getTabCountSupplier()
                                    .get());
                });
        Assert.assertEquals(1, regularTabCount.get());
        Assert.assertEquals(1, incognitoTabCount.get());
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testSingleTabReparentingIntent_PinnedTab() {
        AtomicInteger initialTabCount = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> initialTabCount.set(mActivity.getCurrentTabModel().getCount()));

        Intent reparentingIntent = new Intent(Intent.ACTION_VIEW);
        reparentingIntent.setClass(mActivity, ChromeTabbedActivity.class);
        reparentingIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        reparentingIntent.setData(Uri.parse(JUnitTestGURLs.URL_1.getSpec()));
        IntentHandler.setTabId(reparentingIntent, 101);
        IntentHandler.setPinnedState(reparentingIntent, true);
        IntentUtils.addTrustedIntentExtras(reparentingIntent);

        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onNewIntent(reparentingIntent));

        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(
                            "A new tab should be created.",
                            tabModel.getCount(),
                            Matchers.is(initialTabCount.get() + 1));
                    // Pinned tabs are added to the start of the tab model.
                    Tab tab = tabModel.getTabAt(initialTabCount.get() - 1);
                    Criteria.checkThat(
                            "The URL of the new tab should be correct.",
                            tab.getUrl().getSpec(),
                            Matchers.is(JUnitTestGURLs.URL_1.getSpec()));
                    Criteria.checkThat(
                            "The new tab should be pinned.", tab.getIsPinned(), Matchers.is(true));
                });
    }

    @Test
    @MediumTest
    // Intentionally not batched due to recreating activity.
    @RequiresRestart
    @DisabledTest(message = "crbug.com/1187320 This doesn't work with FeedV2 and crbug.com/1096295")
    public void testActivityCanBeGarbageCollectedAfterFinished() {
        WeakReference<ChromeTabbedActivity> activityRef =
                new WeakReference<>(mActivityTestRule.getActivity());

        ChromeTabbedActivity activity =
                ApplicationTestUtils.recreateActivity(mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        mActivityTestRule.getActivityTestRule().setActivity(activity);

        CriteriaHelper.pollUiThread(
                () -> GarbageCollectionTestUtils.canBeGarbageCollected(activityRef));
    }

    @Test
    @MediumTest
    public void testBackShouldCloseTab() {
        mActivityTestRule.getTestServer(); // Triggers the lazy initialization of the test server.
        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            ChromeTabCreator tabCreator = mActivity.getCurrentTabCreator();
                            return tabCreator.createNewTab(
                                    new LoadUrlParams(
                                            mActivityTestRule.getTestServer().getURL(FILE_PATH)),
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
        mActivityTestRule.getTestServer(); // Triggers the lazy initialization of the test server.
        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            ChromeTabCreator tabCreator = mActivity.getCurrentTabCreator();
                            Tab newTab =
                                    tabCreator.createNewTab(
                                            new LoadUrlParams(
                                                    mActivityTestRule
                                                            .getTestServer()
                                                            .getURL(FILE_PATH)),
                                            TabLaunchType.FROM_LINK,
                                            null);
                            TabGroupModelFilter filter =
                                    mActivity
                                            .getTabModelSelector()
                                            .getTabGroupModelFilterProvider()
                                            .getTabGroupModelFilter(false);
                            filter.createSingleTabGroup(newTab);
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
        when(mTabGroupSyncService.isObservingLocalChanges()).thenReturn(true);

        boolean ret =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity.backShouldCloseTab(tab);
                        });
        Assert.assertFalse(ret);
    }

    private void testTabGroupIntent(boolean shouldApplyCollapse) {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.Reparent.TabGroup.GroupSize", 3)
                        .expectIntRecord("Android.Reparent.TabGroup.GroupSize.Diff", 0)
                        .expectAnyRecord("Android.Reparent.TabGroup.Duration")
                        .build();
        long startTime = SystemClock.elapsedRealtime();
        int initialWindowCount =
                MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY);
        Intent intent =
                new Intent(Intent.ACTION_VIEW, Uri.parse(JUnitTestGURLs.EXAMPLE_URL.getSpec()));
        intent.putExtra(IntentHandler.EXTRA_REPARENT_START_TIME, startTime);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.setClass(mActivity, ChromeTabbedActivity.class);
        IntentHandler.setTabGroupMetadata(intent, createTabGroupMetadata());

        // The newly created ChromeTabbedActivity (created via #startActivity()) should be
        // destroyed, and the intent should be launched in the existing ChromeTabbedActivity.
        ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class,
                Stage.DESTROYED,
                () -> mActivity.getApplicationContext().startActivity(intent));

        Assert.assertEquals(
                "No new window should be opened.",
                initialWindowCount,
                MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY));

        // An individual tab and 3 grouped tabs should be opened in the existing
        // ChromeTabbedActivity.
        CriteriaHelper.pollUiThread(
                () -> {
                    // Verify 4 tabs opened in tab model.
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(4));

                    // Verify urls of the grouped tabs in reverse order.
                    Criteria.checkThat(
                            tabModel.getTabAt(1).getUrl().getSpec(),
                            Matchers.equalTo(TAB_IDS_TO_URLS.get(2).getValue()));
                    Criteria.checkThat(
                            tabModel.getTabAt(2).getUrl().getSpec(),
                            Matchers.equalTo(TAB_IDS_TO_URLS.get(1).getValue()));
                    Criteria.checkThat(
                            tabModel.getTabAt(3).getUrl().getSpec(),
                            Matchers.equalTo(TAB_IDS_TO_URLS.get(0).getValue()));

                    // Verify the tabs are grouped with the correct rootId and tabGroupId.
                    int expectedRootId = tabModel.getTabAt(1).getId();
                    for (int i = 1; i < tabModel.getCount() - 1; i++) {
                        Tab curTab = tabModel.getTabAt(i);
                        Assert.assertEquals(
                                "tabGroupId is incorrect", TAB_GROUP_ID, curTab.getTabGroupId());
                        // Tab collection no longer uses rootId.
                        if (!ChromeFeatureList.sTabCollectionAndroid.isEnabled()) {
                            Assert.assertEquals(
                                    "rootId is incorrect", expectedRootId, curTab.getRootId());
                        }
                    }

                    // Verify other tab group properties.
                    TabGroupModelFilter filter =
                            mActivity
                                    .getTabModelSelector()
                                    .getTabGroupModelFilterProvider()
                                    .getTabGroupModelFilter(false);
                    Assert.assertEquals(TAB_GROUP_TITLE, filter.getTabGroupTitle(TAB_GROUP_ID));
                    Assert.assertEquals(0, filter.getTabGroupColor(TAB_GROUP_ID));
                    if (shouldApplyCollapse) {
                        Assert.assertTrue(filter.getTabGroupCollapsed(TAB_GROUP_ID));
                    } else {
                        Assert.assertFalse(filter.getTabGroupCollapsed(TAB_GROUP_ID));
                    }

                    // Verify histograms.
                    histogramExpectation.assertExpected();
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testMultiUrlReparentingIntent() {
        AtomicInteger initialTabCount = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> initialTabCount.set(mActivity.getCurrentTabModel().getCount()));

        Intent reparentingIntent = new Intent(Intent.ACTION_VIEW);
        reparentingIntent.setClass(mActivity, ChromeTabbedActivity.class);
        reparentingIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        IntentHandler.setMultiTabMetadata(
                reparentingIntent,
                MultiTabMetadata.createForTesting(
                        /* tabIds= */ new ArrayList<>(List.of(101, 102)),
                        /* urls= */ new ArrayList<>(
                                List.of(
                                        JUnitTestGURLs.URL_1.getSpec(),
                                        JUnitTestGURLs.URL_2.getSpec())),
                        /* isPinned= */ new boolean[] {false, false},
                        /* isIncognito= */ false));

        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onNewIntent(reparentingIntent));

        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(initialTabCount.get() + 2));
                    // Tabs are added at the end of the tab model.
                    Criteria.checkThat(
                            tabModel.getTabAt(initialTabCount.get()).getUrl(),
                            Matchers.is(JUnitTestGURLs.URL_1));
                    Criteria.checkThat(
                            tabModel.getTabAt(initialTabCount.get() + 1).getUrl(),
                            Matchers.is(JUnitTestGURLs.URL_2));
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testMultiUrlReparentingIntent_EmptyList() {
        AtomicInteger initialTabCount = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> initialTabCount.set(mActivity.getCurrentTabModel().getCount()));

        Intent reparentingIntent = new Intent(Intent.ACTION_VIEW);
        reparentingIntent.setClass(mActivity, ChromeTabbedActivity.class);
        reparentingIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        IntentHandler.setMultiTabMetadata(
                reparentingIntent,
                MultiTabMetadata.createForTesting(
                        /* tabIds= */ new ArrayList<>(),
                        /* urls= */ new ArrayList<>(),
                        /* isPinned= */ new boolean[0],
                        /* isIncognito= */ false));

        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onNewIntent(reparentingIntent));

        // Wait to ensure no new tabs are created.
        SystemClock.sleep(1000);

        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(initialTabCount.get()));
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testMultiUrlReparentingIntent_mismatchedLists() {
        AtomicInteger initialTabCount = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> initialTabCount.set(mActivity.getCurrentTabModel().getCount()));

        Intent reparentingIntent = new Intent(Intent.ACTION_VIEW);
        reparentingIntent.setClass(mActivity, ChromeTabbedActivity.class);
        reparentingIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Mismatch: 2 IDs, 1 URL. This should be handled gracefully without crashing.
        IntentHandler.setMultiTabMetadata(
                reparentingIntent,
                MultiTabMetadata.createForTesting(
                        /* tabIds= */ new ArrayList<>(List.of(101, 102)),
                        /* urls= */ new ArrayList<>(List.of(JUnitTestGURLs.URL_1.getSpec())),
                        /* isPinned= */ new boolean[] {false, false},
                        /* isIncognito= */ false));

        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onNewIntent(reparentingIntent));

        // Wait to ensure no new tabs are created.
        SystemClock.sleep(500);

        // Verify that no new tabs were created due to the malformed intent.
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(
                            "Tab count should not change for mismatched lists",
                            tabModel.getCount(),
                            Matchers.is(initialTabCount.get()));
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.VANILLA_ICE_CREAM)
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testLaunchIncognitoWindowWithExtras_NightModeDefaultEnabled() {
        // This is Android V+ because overriding night mode requires the intent to be stored
        // by attachBaseContext(), which is not the case for versions below Android V.

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setClass(mActivity, ChromeTabbedActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        IntentUtils.addTrustedIntentExtras(intent);

        final ChromeTabbedActivity incognitoWindowActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> mActivity.getApplicationContext().startActivity(intent));

        // Incognito activity should be in night mode by default.
        NightModeStateProvider incognitoWindowNightModeStateProvider =
                ThreadUtils.runOnUiThreadBlocking(
                        incognitoWindowActivity::createNightModeStateProvider);
        Assert.assertTrue(
                incognitoWindowNightModeStateProvider
                        instanceof IncognitoWindowNightModeStateProvider);
        Assert.assertTrue(incognitoWindowNightModeStateProvider.isInNightMode());
        Assert.assertEquals(
                "AppCompatDelegate should be set to local night mode",
                AppCompatDelegate.MODE_NIGHT_YES,
                incognitoWindowActivity.getDelegate().getLocalNightMode());
    }

    private TabGroupMetadata createTabGroupMetadata() {
        return new TabGroupMetadata(
                /* selectedTabId= */ 1,
                /* sourceWindowId= */ 1,
                TAB_GROUP_ID,
                TAB_IDS_TO_URLS,
                /* tabGroupColor= */ 0,
                TAB_GROUP_TITLE,
                /* mhtmlTabTitle= */ null,
                /* tabGroupCollapsed= */ true,
                /* isGroupShared= */ false,
                /* isIncognito= */ false);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testMultiUrlReparentingIntent_PinnedTabs() {
        AtomicInteger initialTabCount = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> initialTabCount.set(mActivity.getCurrentTabModel().getCount()));

        Intent reparentingIntent = new Intent(Intent.ACTION_VIEW);
        reparentingIntent.setClass(mActivity, ChromeTabbedActivity.class);
        reparentingIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        IntentHandler.setMultiTabMetadata(
                reparentingIntent,
                MultiTabMetadata.createForTesting(
                        /* tabIds= */ new ArrayList<>(List.of(101, 102)),
                        /* urls= */ new ArrayList<>(
                                List.of(
                                        JUnitTestGURLs.URL_1.getSpec(),
                                        JUnitTestGURLs.URL_2.getSpec())),
                        /* isPinned= */ new boolean[] {true, false},
                        /* isIncognito= */ false));

        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onNewIntent(reparentingIntent));

        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(initialTabCount.get() + 2));
                    // Tabs are added at the end of the tab model.
                    // Pinned tab is added to the start.
                    Tab firstTab = tabModel.getTabAt(initialTabCount.get() - 1);
                    Tab secondTab = tabModel.getTabAt(initialTabCount.get() + 1);
                    Criteria.checkThat(firstTab.getUrl(), Matchers.is(JUnitTestGURLs.URL_1));
                    Criteria.checkThat(secondTab.getUrl(), Matchers.is(JUnitTestGURLs.URL_2));
                    Criteria.checkThat(firstTab.getIsPinned(), Matchers.is(true));
                    Criteria.checkThat(secondTab.getIsPinned(), Matchers.is(false));
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testMaybeLaunchDraggedMultiTabInWindow() {
        AtomicInteger initialTabCount = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> initialTabCount.set(mActivity.getCurrentTabModel().getCount()));

        Intent dragIntent = new Intent(Intent.ACTION_VIEW);
        dragIntent.setClass(mActivity, ChromeTabbedActivity.class);
        dragIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        IntentHandler.setMultiTabMetadata(
                dragIntent,
                MultiTabMetadata.createForTesting(
                        /* tabIds= */ new ArrayList<>(List.of(201, 202)),
                        /* urls= */ new ArrayList<>(
                                List.of(
                                        JUnitTestGURLs.URL_1.getSpec(),
                                        JUnitTestGURLs.URL_2.getSpec())),
                        /* isPinned= */ new boolean[] {false, false},
                        /* isIncognito= */ false));

        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onNewIntent(dragIntent));

        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(initialTabCount.get() + 2));
                    // Tabs are added at the end of the tab model.
                    Criteria.checkThat(
                            tabModel.getTabAt(initialTabCount.get()).getUrl(),
                            Matchers.is(JUnitTestGURLs.URL_1));
                    Criteria.checkThat(
                            tabModel.getTabAt(initialTabCount.get() + 1).getUrl(),
                            Matchers.is(JUnitTestGURLs.URL_2));
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testMaybeLaunchDraggedMultiTabInWindow_PinnedTabs() {
        AtomicInteger initialTabCount = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> initialTabCount.set(mActivity.getCurrentTabModel().getCount()));

        Intent dragIntent = new Intent(Intent.ACTION_VIEW);
        dragIntent.setClass(mActivity, ChromeTabbedActivity.class);
        dragIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        IntentHandler.setMultiTabMetadata(
                dragIntent,
                MultiTabMetadata.createForTesting(
                        /* tabIds= */ new ArrayList<>(List.of(201, 202)),
                        /* urls= */ new ArrayList<>(
                                List.of(
                                        JUnitTestGURLs.URL_1.getSpec(),
                                        JUnitTestGURLs.URL_2.getSpec())),
                        /* isPinned= */ new boolean[] {true, false},
                        /* isIncognito= */ false));

        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onNewIntent(dragIntent));

        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(initialTabCount.get() + 2));
                    // Tabs are added at the end of the tab model.
                    // Pinned tab is added to the start.
                    Tab firstTab = tabModel.getTabAt(initialTabCount.get() - 1);
                    Tab secondTab = tabModel.getTabAt(initialTabCount.get() + 1);
                    Criteria.checkThat(firstTab.getUrl(), Matchers.is(JUnitTestGURLs.URL_1));
                    Criteria.checkThat(secondTab.getUrl(), Matchers.is(JUnitTestGURLs.URL_2));
                    Criteria.checkThat(firstTab.getIsPinned(), Matchers.is(true));
                    Criteria.checkThat(secondTab.getIsPinned(), Matchers.is(false));
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testMaybeLaunchDraggedMultiTabInWindow_EmptyList() {
        AtomicInteger initialTabCount = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> initialTabCount.set(mActivity.getCurrentTabModel().getCount()));

        Intent dragIntent = new Intent(Intent.ACTION_VIEW);
        dragIntent.setClass(mActivity, ChromeTabbedActivity.class);
        dragIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        IntentHandler.setMultiTabMetadata(
                dragIntent,
                MultiTabMetadata.createForTesting(
                        /* tabIds= */ new ArrayList<>(),
                        /* urls= */ new ArrayList<>(),
                        /* isPinned= */ new boolean[0],
                        /* isIncognito= */ false));

        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onNewIntent(dragIntent));

        // Wait to ensure no new tabs are created.
        SystemClock.sleep(500);

        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(tabModel.getCount(), Matchers.is(initialTabCount.get()));
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testMaybeLaunchDraggedMultiTabInWindow_mismatchedLists() {
        AtomicInteger initialTabCount = new AtomicInteger();
        ThreadUtils.runOnUiThreadBlocking(
                () -> initialTabCount.set(mActivity.getCurrentTabModel().getCount()));

        Intent dragIntent = new Intent(Intent.ACTION_VIEW);
        dragIntent.setClass(mActivity, ChromeTabbedActivity.class);
        dragIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        IntentHandler.setMultiTabMetadata(
                dragIntent,
                MultiTabMetadata.createForTesting(
                        /* tabIds= */ new ArrayList<>(List.of(201, 202)),
                        /* urls= */ new ArrayList<>(List.of(JUnitTestGURLs.URL_1.getSpec())),
                        /* isPinned= */ new boolean[] {false, false},
                        /* isIncognito= */ false));

        ThreadUtils.runOnUiThreadBlocking(() -> mActivity.onNewIntent(dragIntent));

        // Wait to ensure no new tabs are created.
        SystemClock.sleep(500);

        // Verify that no new tabs were created due to the malformed intent.
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel = mActivity.getCurrentTabModel();
                    Criteria.checkThat(
                            "Tab count should not change for mismatched lists",
                            tabModel.getCount(),
                            Matchers.is(initialTabCount.get()));
                });
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void testMoveTabsToOtherWindowAndMerge() {
        // 1. Launch a second ChromeTabbedActivity.
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.setClass(mActivity, ChromeTabbedActivity.class);

        final ChromeTabbedActivity activity2 =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () -> mActivity.getApplicationContext().startActivity(intent));

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);

        // 2. Create two tabs in the first activity.
        Tab tab1 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getTabCreator(false)
                                    .createNewTab(
                                            new LoadUrlParams(JUnitTestGURLs.URL_1),
                                            TabLaunchType.FROM_CHROME_UI,
                                            null);
                        });
        Tab tab2 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getTabCreator(false)
                                    .createNewTab(
                                            new LoadUrlParams(JUnitTestGURLs.URL_2),
                                            TabLaunchType.FROM_CHROME_UI,
                                            null);
                        });

        // 3. Get MultiInstanceManager for activity1 and InstanceInfo for activity2.
        MultiInstanceManager mim1 = mActivity.getMultiInstanceMangerForTesting();

        final AtomicReference<InstanceInfo> instanceInfo2Ref = new AtomicReference<>();
        CriteriaHelper.pollUiThread(
                () -> {
                    List<InstanceInfo> instanceInfos = mim1.getInstanceInfo();
                    for (InstanceInfo info : instanceInfos) {
                        if (info.taskId == activity2.getTaskId()) {
                            instanceInfo2Ref.set(info);
                            return true;
                        }
                    }
                    return false;
                },
                "Could not find InstanceInfo for activity2");
        InstanceInfo instanceInfo2 = instanceInfo2Ref.get();

        // 4. Move tab1 to activity2.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mim1.moveTabsToWindow(
                            instanceInfo2, List.of(tab1), -1, NewWindowAppSource.OTHER);
                });

        // 5. Verify tab1 is in activity2.
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel2 = activity2.getCurrentTabModel();
                    Criteria.checkThat(tabModel2.getCount(), Matchers.is(2)); // 1 blank + tab1
                    Criteria.checkThat(tabModel2.getTabById(tab1.getId()), Matchers.notNullValue());
                });

        // 6. Move tab2 to activity2 and merge with tab1.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mim1.moveTabsToWindowAndMergeToDest(instanceInfo2, List.of(tab2), tab1.getId());
                });

        // 7. Verify tab2 is in activity2 and merged with tab1.
        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel tabModel2 = activity2.getCurrentTabModel();
                    Criteria.checkThat(
                            tabModel2.getCount(), Matchers.is(3)); // 1 blank + tab1 + tab2
                    Tab movedTab2 = tabModel2.getTabById(tab2.getId());
                    Criteria.checkThat(movedTab2, Matchers.notNullValue());

                    TabGroupModelFilter filter2 =
                            (TabGroupModelFilter)
                                    activity2
                                            .getTabModelSelector()
                                            .getTabGroupModelFilterProvider()
                                            .getTabGroupModelFilter(false);
                    List<Tab> relatedTabs = filter2.getRelatedTabList(tab1.getId());
                    Criteria.checkThat(relatedTabs.size(), Matchers.is(2));
                    Criteria.checkThat(
                            relatedTabs,
                            Matchers.hasItems(tabModel2.getTabById(tab1.getId()), movedTab2));
                });
    }
}
