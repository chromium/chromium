// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.Collections;
import java.util.concurrent.TimeUnit;

/** Tests for {@link TabArchiverImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPostTask.class})
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
    private @Mock TabGroupSyncService mTabGroupSyncService;
    private @Mock TabRemover mTabRemover;
    private @Mock TabRemover mIncogTabRemover;

    private MockTabModelSelector mTabModelSelector;
    private TabArchiverImpl mTabArchiver;

    @Before
    public void setUp() {
        // Run posted tasks immediately.
        ShadowPostTask.setTestImpl((taskTraits, task, delay) -> task.run());

        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);

        // Testing setup is:
        // 50 regular tabs, 10 incognito tabs.
        // Clock is setup 1 hour past epoch 0.
        // Tab timestamps set at epoch 0.
        setupTabModels();
        setupTabsForArchive();
        mTabArchiver =
                new TabArchiverImpl(
                        mArchivedTabGroupModelFilter,
                        mArchivedTabCreator,
                        mTabArchiveSettings,
                        mClock,
                        mTabGroupSyncService);
    }

    private void setupTabModels() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        // Setup the archived tab model filter to always return a mock TabModel. This behavior can
        // be overridden in tests to test if an archived tab exists in the regular tab model.
        doReturn(mArchivedTabModel).when(mArchivedTabGroupModelFilter).getTabModel();
        MockTab tab = new MockTab(0, mProfile);
        doReturn(tab).when(mArchivedTabCreator).createFrozenTab(any(), anyInt(), anyInt());
        doAnswer(inv -> Collections.emptyList().iterator()).when(mArchivedTabModel).iterator();

        mTabModelSelector =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 50, 10, this::createTab);
        mTabModelSelector.markTabStateInitialized();

        MockTabModel regularModel =
                (MockTabModel) mTabModelSelector.getModel(/* incognito= */ false);
        regularModel.setTabRemoverForTesting(mTabRemover);
        MockTabModel incognitoModel =
                (MockTabModel) mTabModelSelector.getModel(/* incognito= */ true);
        incognitoModel.setTabRemoverForTesting(mIncogTabRemover);
    }

    private MockTab createTab(int id, boolean incognito) {
        Profile profile = incognito ? mIncognitoProfile : mProfile;
        return MockTab.createAndInitialize(id, profile);
    }

    private void setupTabsForArchive() {
        doReturn(true).when(mTabArchiveSettings).getArchiveEnabled();
        // Set the tab to expire after 2 hour to simplify testing.
        doReturn(2).when(mTabArchiveSettings).getArchiveTimeDeltaHours();

        // Set the clock to 2 hour after 0.
        doReturn(TimeUnit.HOURS.toMillis(2)).when(mClock).currentTimeMillis();
        TabList regularTabs =
                mTabModelSelector.getModel(/* incognito= */ false).getComprehensiveModel();
        for (int i = 0; i < regularTabs.getCount(); i++) {
            TabImpl tab = (TabImpl) regularTabs.getTabAt(i);
            tab.setTimestampMillis(0L);
            // Set the navigation timestamp for both tabs at 1 to pass user active check.
            tab.setLastNavigationCommittedTimestampMillis(TimeUnit.HOURS.toMillis(1));
            // Always return a tab state for each regular tab to unblock archiving.
            TabState tabState = new TabState();
            tabState.contentsState = mWebContentsState;
            TabStateExtractor.setTabStateForTesting(tab.getId(), tabState);
        }
    }

    @Test
    public void testMaxSimultaneousArchives() {
        when(mTabArchiveSettings.getMaxSimultaneousArchives()).thenReturn(20);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Tabs.ArchivedTabs.MaxLimitReachedAt", 20);

        mTabArchiver.doArchivePass(mTabModelSelector);

        verify(mArchivedTabCreator, times(20)).createFrozenTab(any(), anyInt(), anyInt());
        watcher.assertExpected();
    }

    @Test
    public void testDoArchivePass_currentModelIsIncognito() {
        when(mTabArchiveSettings.getMaxSimultaneousArchives()).thenReturn(1);
        mTabModelSelector.selectModel(/* incognito= */ true);

        mTabArchiver.doArchivePass(mTabModelSelector);

        verify(mArchivedTabCreator).createFrozenTab(any(), anyInt(), anyInt());
        verify(mTabRemover).closeTabs(any(), anyBoolean());
        verify(mIncogTabRemover, never()).closeTabs(any(), anyBoolean());
    }
}
