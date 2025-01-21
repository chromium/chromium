// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;

/** Tests for {@link TabArchiveSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPostTask.class})
@EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER)
public class TabArchiverUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    private @Mock TabGroupModelFilter mArchivedTabGroupModelFilter;
    private @Mock TabModel mArchivedTabModel;
    private @Mock TabCreator mArchivedTabCreator;
    private @Mock TabArchiveSettings mTabArchiveSettings;
    private @Mock TabArchiverImpl.Clock mClock;
    private @Mock Profile mProfile;
    private @Mock Profile mIncognitoProfile;
    private @Mock WebContentsState mWebContentsState;

    private MockTab mArchivedTab;
    private MockTabModelSelector mTabModelSelector;
    private TabArchiverImpl mTabArchiver;

    @Before
    public void setUp() {
        // Run posted tasks immediately.
        ShadowPostTask.setTestImpl(
                new TestImpl() {
                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        task.run();
                    }
                });

        // Testing setup is:
        // 50 regular tabs, 0 incognito tabs.
        // Clock is setup 1 hour past epoch 0.
        // Tab timestamps set at epoch 0.
        setupTabModels();
        setupTabsForArchive();
        mTabArchiver =
                new TabArchiverImpl(
                        mArchivedTabGroupModelFilter,
                        mArchivedTabCreator,
                        mTabArchiveSettings,
                        mClock);
    }

    private void setupTabModels() {
        // Setup the archived tab model filter to always return a mock TabModel. This behavior can
        // be overridden in tests to test if an archived tab exists in the regular tab model.
        doReturn(mArchivedTabModel).when(mArchivedTabGroupModelFilter).getTabModel();
        mArchivedTab = new MockTab(0, mProfile);
        doReturn(mArchivedTab).when(mArchivedTabCreator).createFrozenTab(any(), anyInt(), anyInt());

        // Setup the regular tab models
        mTabModelSelector =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 50, 0, /* delegate= */ null);
        mTabModelSelector.markTabStateInitialized();
    }

    private void setupTabsForArchive() {
        doReturn(true).when(mTabArchiveSettings).getArchiveEnabled();
        doReturn(0).when(mTabArchiveSettings).getArchiveTimeDeltaHours();

        // The clock should be one hour after epoch 0.
        doReturn(0L).when(mClock).currentTimeMillis();
        TabList regularTabs =
                mTabModelSelector.getModel(/* incognito= */ false).getComprehensiveModel();
        for (int i = 0; i < regularTabs.getCount(); i++) {
            Tab tab = regularTabs.getTabAt(i);
            tab.setTimestampMillis(0L);
            // Always return a tab state for each regular tab to unblock archiving.
            TabState tabState = new TabState();
            tabState.contentsState = mWebContentsState;
            TabStateExtractor.setTabStateForTesting(tab.getId(), tabState);
        }
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_TAB_DECLUTTER
                    + ":android_tab_declutter_max_simultaneous_archives/20")
    public void testMaxSimultaneousArchives() {
        assertEquals(20, ChromeFeatureList.sAndroidTabDeclutterMaxSimultaneousArchives.getValue());

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Tabs.ArchivedTabs.MaxLimitReachedAt", 20);
        mTabArchiver.doArchivePass(mTabModelSelector);
        verify(mArchivedTabCreator, times(20)).createFrozenTab(any(), anyInt(), anyInt());
        watcher.assertExpected();
    }
}
