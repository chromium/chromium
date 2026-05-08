// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.AdditionalMatchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.concurrent.TimeUnit;

/** Tests for {@link TabArchiverImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabArchiverUnitTest {
    private static final GURL TEST_GURL = new GURL("https://www.google.com");
    private static final GURL TEST_GURL_2 = new GURL("https://www.example.com");

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

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
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        TabIdManager.resetInstanceForTesting();

        // Testing setup is:
        // 50 regular tabs, 10 incognito tabs.
        // Clock is setup 1 hour past epoch 0.
        // Tab timestamps set at epoch 0.
        setupTabModels();
        setupTabsForArchive();
        mTabArchiver =
                new TabArchiverImpl(
                        mArchivedTabModel,
                        mArchivedTabCreator,
                        mTabArchiveSettings,
                        mClock,
                        mTabGroupSyncService);
    }

    private void setupTabModels() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        // Setup the archived tab model. This behavior can be overridden in tests to test if an
        // archived tab exists in the regular tab model.
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

    @Test
    public void testArchiveAndRemoveTabs_TabIdAlreadyArchived_SameUrl() {
        MockTab tab = (MockTab) mTabModelSelector.getModel(/* incognito= */ false).getTabAt(0);
        tab.setGurlOverrideForTesting(TEST_GURL);

        MockTab archivedTab = new MockTab(tab.getId(), mProfile);
        archivedTab.setGurlOverrideForTesting(TEST_GURL);
        when(mArchivedTabModel.getTabById(tab.getId())).thenReturn(archivedTab);

        mTabArchiver.archiveAndRemoveTabs(
                mTabModelSelector.getModel(/* incognito= */ false), Collections.singletonList(tab));

        verify(mArchivedTabCreator, never()).createFrozenTab(any(), anyInt(), anyInt());
    }

    @Test
    public void testArchiveAndRemoveTabs_TabIdAlreadyArchived_DifferentUrl() {
        MockTab tab = (MockTab) mTabModelSelector.getModel(/* incognito= */ false).getTabAt(0);
        tab.setGurlOverrideForTesting(TEST_GURL);

        MockTab archivedTab = new MockTab(tab.getId(), mProfile);
        archivedTab.setGurlOverrideForTesting(TEST_GURL_2);
        when(mArchivedTabModel.getTabById(tab.getId())).thenReturn(archivedTab);

        mTabArchiver.archiveAndRemoveTabs(
                mTabModelSelector.getModel(/* incognito= */ false), Collections.singletonList(tab));

        verify(mArchivedTabCreator, times(1))
                .createFrozenTab(any(), not(eq(tab.getId())), anyInt());
    }

    @Test
    public void testDoArchivePass_NoTabsArchived_TriggersPersistedTabDataCreated() {
        // Setup tabs so that none are eligible for archive.
        TabList regularTabs =
                mTabModelSelector.getModel(/* incognito= */ false).getComprehensiveModel();
        for (int i = 0; i < regularTabs.getCount(); i++) {
            TabImpl tab = (TabImpl) regularTabs.getTabAt(i);
            tab.setTimestampMillis(TimeUnit.HOURS.toMillis(2)); // Same as clock
        }

        TabArchiver.Observer observer = mock(TabArchiver.Observer.class);
        mTabArchiver.addObserver(observer);

        mTabArchiver.doArchivePass(mTabModelSelector);
        shadowOf(Looper.getMainLooper()).idle();
        verify(observer).onArchivePersistedTabDataCreated();
    }
}
