// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.tabmodel.TabList.INVALID_TAB_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;

import android.util.Pair;

import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherSearchTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.omnibox.OmniboxFeatureList;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for ArchivedTabModelOrchestrator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Test interacts with activity shutdown and thus is incompatible with batching")
@EnableFeatures({
    ChromeFeatureList.ANDROID_TAB_DECLUTTER,
    ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH
})
@DisableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_ALL_BUT_ACTIVE)
public class ArchivedTabModelOrchestratorTest {
    private static final String TEST_PATH = "/chrome/test/data/android/about.html";
    private static final String TEST_PATH_2 = "/chrome/test/data/android/google.html";

    private static class FakeTaskRunner implements TaskRunner {

        public final List<Pair<Runnable, Long>> mDelayedTasks = new ArrayList<>();

        @Override
        public void execute(Runnable task) {
            assert false;
        }

        @Override
        public void postDelayedTask(Runnable task, long delay) {
            mDelayedTasks.add(new Pair<>(task, delay));
        }
    }

    private static class FakeDeferredStartupHandler extends DeferredStartupHandler {
        private List<Runnable> mTasks = new ArrayList<>();

        @Override
        public void addDeferredTask(Runnable task) {
            mTasks.add(task);
        }

        public void runAllTasks() {
            for (Runnable task : mTasks) {
                task.run();
            }
            mTasks.clear();
        }
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private ArchivedTabModelOrchestrator.Observer mObserver;
    @Mock private TabArchiver.Clock mClock;
    @Mock private ObservableSupplierImpl<Boolean> mSkipSaveTabListSupplier;

    private Profile mProfile;
    private FakeTaskRunner mTaskRunner;
    private FakeDeferredStartupHandler mDeferredStartupHandler;
    private ArchivedTabModelOrchestrator mOrchestrator;
    private TabModel mArchivedTabModel;
    private TabModel mRegularTabModel;
    private TabCreator mRegularTabCreator;
    private TabArchiveSettings mTabArchiveSettings;

    @Before
    public void setUp() throws Exception {
        mDeferredStartupHandler = new FakeDeferredStartupHandler();
        DeferredStartupHandler.setInstanceForTests(mDeferredStartupHandler);
        mActivityTestRule.startMainActivityOnBlankPage();

        runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            mActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    mOrchestrator = ArchivedTabModelOrchestrator.getForProfile(mProfile);
                    mTaskRunner = new FakeTaskRunner();
                    mOrchestrator.setTaskRunnerForTesting(mTaskRunner);
                });
        doReturn(false).when(mSkipSaveTabListSupplier).get();
    }

    private void finishLoading() {
        runOnUiThreadBlocking(
                () -> {
                    mDeferredStartupHandler.runAllTasks();
                    assert mOrchestrator.areTabModelsInitialized();
                    mOrchestrator.getTabArchiveSettings().resetSettingsForTesting();
                    mArchivedTabModel = mOrchestrator.getTabModelSelector().getModel(false);
                    mRegularTabModel = mActivityTestRule.getActivity().getCurrentTabModel();
                    mRegularTabCreator = mActivityTestRule.getActivity().getTabCreator(false);
                    mTabArchiveSettings = mOrchestrator.getTabArchiveSettings();
                    mTabArchiveSettings.setArchiveEnabled(true);
                });
    }

    private void setupDeclutterSettingsForTest() {
        runOnUiThreadBlocking(() -> mTabArchiveSettings.setArchiveTimeDeltaHours(0));
    }

    @Test
    @MediumTest
    public void testDeferredInitialization() {
        assertFalse(mOrchestrator.areTabModelsInitialized());
        runOnUiThreadBlocking(() -> mOrchestrator.addObserver(mObserver));
        finishLoading();
        assertTrue(mOrchestrator.areTabModelsInitialized());
        verify(mObserver).onTabModelCreated(any());
    }

    @Test
    @MediumTest
    public void testBeginDeclutter_DisablesAndEnablesSaveTabListTask() {
        finishLoading();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        setupDeclutterSettingsForTest();
        runOnUiThreadBlocking(() -> mOrchestrator.resetBeginDeclutterForTesting());
        runOnUiThreadBlocking(
                () -> {
                    mOrchestrator.setSkipSaveTabListSupplierForTesting(mSkipSaveTabListSupplier);
                    mOrchestrator.maybeBeginDeclutter();
                });

        CriteriaHelper.pollUiThread(() -> 2 == mTaskRunner.mDelayedTasks.size());
        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());

        verify(mSkipSaveTabListSupplier).set(true);
        verify(mSkipSaveTabListSupplier).set(false);
    }

    @Test
    @MediumTest
    public void testDeclutterInactiveTabs() {
        finishLoading();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        runOnUiThreadBlocking(
                () -> {
                    doReturn(TimeUnit.HOURS.toMillis(2)).when(mClock).currentTimeMillis();
                    mOrchestrator.getTabArchiver().setClockForTesting(mClock);
                    mRegularTabModel.getTabAt(0).setTimestampMillis(0L);
                    mRegularTabModel.getTabAt(1).setTimestampMillis(0L);
                    mTabArchiveSettings.setArchiveTimeDeltaHours(1);
                    mOrchestrator.resetBeginDeclutterForTesting();
                    mOrchestrator.maybeBeginDeclutter();
                });

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        runOnUiThreadBlocking(
                () ->
                        mOrchestrator
                                .getTabArchiver()
                                .unarchiveAndRestoreTabs(
                                        mRegularTabCreator,
                                        Arrays.asList(mArchivedTabModel.getTabAt(0)),
                                        /* updateTimestamp= */ true));

        // Now the timestamp has been updated, no tabs should get archived.
        runOnUiThreadBlocking(
                () -> {
                    mOrchestrator.resetBeginDeclutterForTesting();
                    mOrchestrator.maybeBeginDeclutter();
                });
        CriteriaHelper.pollUiThread(() -> 2 == mRegularTabModel.getCount());
        CriteriaHelper.pollUiThread(() -> 0 == mArchivedTabModel.getCount());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_ALL_BUT_ACTIVE})
    public void testArchiveAllButActive() {
        finishLoading();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        runOnUiThreadBlocking(() -> mOrchestrator.resetBeginDeclutterForTesting());
        runOnUiThreadBlocking(() -> mOrchestrator.maybeBeginDeclutter());

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
    }

    @Test
    @MediumTest
    public void testScheduledDeclutter() {
        finishLoading();
        runOnUiThreadBlocking(() -> mTabArchiveSettings.setArchiveEnabled(false));
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        setupDeclutterSettingsForTest();
        mTaskRunner.mDelayedTasks.clear();
        runOnUiThreadBlocking(() -> mOrchestrator.resetBeginDeclutterForTesting());
        runOnUiThreadBlocking(() -> mOrchestrator.maybeBeginDeclutter());

        CriteriaHelper.pollUiThread(() -> 1 == mTaskRunner.mDelayedTasks.size());
        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        runOnUiThreadBlocking(() -> mOrchestrator.getTabArchiveSettings().setArchiveEnabled(true));
        // A task was scheduled to perform a scheduled declutter, get it and run it.
        runOnUiThreadBlocking(() -> mTaskRunner.mDelayedTasks.get(0).first.run());

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        // The new tab should be archived now.
        assertEquals(1, mArchivedTabModel.getCount());

        // The schedule call should queue up another runnable.
        assertEquals(2, mTaskRunner.mDelayedTasks.size());
    }

    @Test
    @MediumTest
    public void testRescueTabs_FeatureFlag() {
        finishLoading();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        setupDeclutterSettingsForTest();
        runOnUiThreadBlocking(() -> mOrchestrator.resetBeginDeclutterForTesting());
        runOnUiThreadBlocking(() -> mOrchestrator.maybeBeginDeclutter());

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());

        runOnUiThreadBlocking(() -> mOrchestrator.maybeRescueArchivedTabs());

        CriteriaHelper.pollUiThread(() -> 2 == mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
    }

    @Test
    @MediumTest
    public void testRescueTabs_ArchiveDisabled() {
        finishLoading();
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        setupDeclutterSettingsForTest();
        runOnUiThreadBlocking(() -> mOrchestrator.resetBeginDeclutterForTesting());
        runOnUiThreadBlocking(() -> mOrchestrator.maybeBeginDeclutter());

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());

        runOnUiThreadBlocking(() -> mTabArchiveSettings.setArchiveEnabled(false));

        CriteriaHelper.pollUiThread(() -> mRegularTabModel.getCount() == 2);
        assertEquals(0, mArchivedTabModel.getCount());
    }

    @Test
    @MediumTest
    public void testGetModelIndex() {
        finishLoading();
        assertEquals(INVALID_TAB_INDEX, mArchivedTabModel.index());
    }

    @Test
    @MediumTest
    public void testDestroyBeforeActivityDestroyed() {
        finishLoading();
        runOnUiThreadBlocking(() -> ArchivedTabModelOrchestrator.destroyProfileKeyedMap());
        // The PKM is already destroyed, but the ATMO shouldn't crash when it
        // receives an activity destroyed event.
    }

    @Test
    @MediumTest
    public void testDeclutterAfterDestroy() {
        finishLoading();
        runOnUiThreadBlocking(() -> mOrchestrator.getTabArchiveSettings().setArchiveEnabled(false));
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        setupDeclutterSettingsForTest();
        mTaskRunner.mDelayedTasks.clear();
        runOnUiThreadBlocking(() -> mOrchestrator.resetBeginDeclutterForTesting());
        runOnUiThreadBlocking(() -> mOrchestrator.maybeBeginDeclutter());

        CriteriaHelper.pollUiThread(() -> 1 == mTaskRunner.mDelayedTasks.size());
        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());

        runOnUiThreadBlocking(() -> ArchivedTabModelOrchestrator.destroyProfileKeyedMap());
        runOnUiThreadBlocking(() -> mTaskRunner.mDelayedTasks.get(0).first.run());
        // Running the archive task should have had no effect after the destroy.
        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
    }

    @Test
    @MediumTest
    @EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH)
    public void testOpenArchivedTabFromHubSearch() {
        finishLoading();
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(TEST_PATH));
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH_2), /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        runOnUiThreadBlocking(
                () -> {
                    doReturn(TimeUnit.HOURS.toMillis(2)).when(mClock).currentTimeMillis();
                    mOrchestrator.getTabArchiver().setClockForTesting(mClock);
                    mRegularTabModel.getTabAt(0).setTimestampMillis(0L);
                    mRegularTabModel.getTabAt(1).setTimestampMillis(0L);
                    mTabArchiveSettings.setArchiveTimeDeltaHours(1);
                    mOrchestrator.resetBeginDeclutterForTesting();
                    mOrchestrator.maybeBeginDeclutter();
                });

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);

        onView(
                        allOf(
                                withId(R.id.line_2),
                                withText(containsString(TEST_PATH)),
                                withEffectiveVisibility(Visibility.VISIBLE)))
                .perform(click());
        CriteriaHelper.pollUiThread(() -> 2 == mRegularTabModel.getCount());
    }

    @Test
    @MediumTest
    @EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH)
    public void testOpenArchivedTabFromHubSearch_Incognito() {
        finishLoading();
        mActivityTestRule.loadUrl(mActivityTestRule.getTestServer().getURL(TEST_PATH));
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH_2), /* incognito= */ false);

        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        runOnUiThreadBlocking(
                () -> {
                    doReturn(TimeUnit.HOURS.toMillis(2)).when(mClock).currentTimeMillis();
                    mOrchestrator.getTabArchiver().setClockForTesting(mClock);
                    mRegularTabModel.getTabAt(0).setTimestampMillis(0L);
                    mRegularTabModel.getTabAt(1).setTimestampMillis(0L);
                    mTabArchiveSettings.setArchiveTimeDeltaHours(1);
                    mOrchestrator.resetBeginDeclutterForTesting();
                    mOrchestrator.maybeBeginDeclutter();
                });

        CriteriaHelper.pollUiThread(() -> 1 == mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());

        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH_2), /* incognito= */ true);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);

        onView(allOf(withId(R.id.line_2), withText(containsString(TEST_PATH))))
                .check(doesNotExist());
    }
}
