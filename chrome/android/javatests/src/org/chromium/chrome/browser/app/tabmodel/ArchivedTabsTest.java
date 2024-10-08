// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;


import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.util.Pair;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

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
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.CloseAllTabsHelper;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.ArrayList;
import java.util.List;

/** Tests for ArchivedTabModelOrchestrator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Test interacts with activity shutdown and thus is incompatible with batching")
@EnableFeatures({
    ChromeFeatureList.ANDROID_TAB_DECLUTTER,
    ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH
})
@DisableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_ALL_BUT_ACTIVE)
public class ArchivedTabsTest {
    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

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
        finishLoading();
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

    @Test
    @MediumTest
    public void testCloseAllTabsAndClickUndo() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        runOnUiThreadBlocking(
                () -> {
                    TabCreator archivedTabCreator = mOrchestrator.getArchivedTabCreatorForTesting();
                    archivedTabCreator.createNewTab(
                            new LoadUrlParams("https://google.com"),
                            TabLaunchType.FROM_RESTORE,
                            null);
                });
        CriteriaHelper.pollUiThread(() -> 1 == mArchivedTabModel.getCount());

        TabUiTestHelper.enterTabSwitcher(cta);
        runOnUiThreadBlocking(
                () -> {
                    CloseAllTabsHelper.closeAllTabsHidingTabGroups(
                            cta.getTabModelSelectorSupplier().get(), mRegularTabCreator);
                });
        CriteriaHelper.pollUiThread(() -> 0 == mArchivedTabModel.getCount());

        TabUiTestHelper.verifyUndoBarShowingAndClickUndo();
        CriteriaHelper.pollUiThread(() -> 1 == mArchivedTabModel.getCount());
    }
}
