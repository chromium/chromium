// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator.LeaseReason;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabArchiver;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.ArchivedTabModelSelectorHolder;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link ArchivedTabModelOrchestrator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ArchivedTabModelOrchestratorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private TabPersistentStore mMockTabPersistentStore;
    @Mock private TabPersistentStore mMockShadowTabPersistentStore;
    @Mock private TabModelSelectorBase mMockTabModelSelector;
    @Mock private TabPersistencePolicy mMockTabPersistencePolicy;
    @Mock private Profile mMockProfile;
    @Mock private TabCreator mMockArchivedTabCreator;
    @Mock private WindowAndroid mMockWindowAndroid;
    @Mock private CipherFactory mMockCipherFactory;
    @Mock private TabGroupSyncService mMockTabGroupSyncService;
    @Mock private TabPersistentStore mMockTabbedPersistentStore;

    private ArchivedTabModelOrchestrator mOrchestrator;

    @Before
    public void setUp() {
        when(mMockProfile.getOriginalProfile()).thenReturn(mMockProfile);
        when(mMockProfile.isNativeInitialized()).thenReturn(true);
        when(mMockProfile.shutdownStarted()).thenReturn(false);
        when(mMockProfile.isOffTheRecord()).thenReturn(false);
        when(mMockTabGroupSyncService.getAllGroupIds()).thenReturn(new String[0]);
        mOrchestrator = new ArchivedTabModelOrchestrator(mMockProfile);
        mOrchestrator.initForTesting(
                mMockTabModelSelector,
                mMockTabPersistentStore,
                mMockTabPersistencePolicy,
                mMockShadowTabPersistentStore);
        mOrchestrator.setArchivedTabCreatorForTesting(mMockArchivedTabCreator);
        mOrchestrator.setWindowAndroidForTesting(mMockWindowAndroid);
        mOrchestrator.setCipherFactoryForTesting(mMockCipherFactory);
        mOrchestrator.setTabGroupSyncServiceForTesting(mMockTabGroupSyncService);
    }

    @After
    public void tearDown() {
        ArchivedTabModelOrchestrator.getActivityTabModelOrchestratorsForTesting().clear();
        ArchivedTabModelOrchestrator.destroyProfileKeyedMap();
        ArchivedTabModelOrchestrator.setInstanceForTesting(null);
        TabArchiveSettings.getInstance().resetSettingsForTesting();
        TabArchiveSettings.setInstanceForTesting(null);
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }

    @Test
    public void testLoadStatePassesIgnoreRegularFiles() {
        mOrchestrator.loadState(
                /* ignoreIncognitoFiles= */ true,
                /* ignoreRegularFiles= */ true,
                /* onStandardActiveIndexRead= */ null);

        verify(mMockTabPersistentStore).loadState(eq(true), eq(true));
    }

    @Test
    public void testLoadStatePassesNoIgnoreRegularFiles() {
        mOrchestrator.loadState(
                /* ignoreIncognitoFiles= */ true,
                /* ignoreRegularFiles= */ false,
                /* onStandardActiveIndexRead= */ null);

        verify(mMockTabPersistentStore).loadState(eq(true), eq(false));
    }

    @Test
    public void testGetTabCountSupplier() {
        assertEquals(0, mOrchestrator.getTabCountSupplier().get().intValue());

        mOrchestrator.getTabArchiveSettings().setArchivedTabCount(3);
        assertEquals(3, mOrchestrator.getTabCountSupplier().get().intValue());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testAcquireLeaseAndRelease() {
        ArchivedTabModelOrchestrator.setInstanceForTesting(mOrchestrator);

        assertEquals(0, mOrchestrator.getLeaseCountForTesting());
        assertNull(mOrchestrator.getGracePeriodTeardownRunnableForTesting());

        Destroyable lease1 =
                ArchivedTabModelOrchestrator.acquireLease(mMockProfile, LeaseReason.FOR_TESTING);
        assertNotNull(lease1);
        assertEquals(1, mOrchestrator.getLeaseCountForTesting());
        assertNull(mOrchestrator.getGracePeriodTeardownRunnableForTesting());

        Destroyable lease2 =
                ArchivedTabModelOrchestrator.acquireLease(mMockProfile, LeaseReason.FOR_TESTING);
        assertNotNull(lease2);
        assertEquals(2, mOrchestrator.getLeaseCountForTesting());

        lease1.destroy();
        assertEquals(1, mOrchestrator.getLeaseCountForTesting());
        assertNull(mOrchestrator.getGracePeriodTeardownRunnableForTesting());

        // Repeated destruction of the same token does not decrement lease count further.
        lease1.destroy();
        assertEquals(1, mOrchestrator.getLeaseCountForTesting());

        lease2.destroy();
        assertEquals(0, mOrchestrator.getLeaseCountForTesting());
        assertNotNull(mOrchestrator.getGracePeriodTeardownRunnableForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testGracePeriodCancelledOnNewLease() {
        ArchivedTabModelOrchestrator.setInstanceForTesting(mOrchestrator);

        Destroyable lease =
                ArchivedTabModelOrchestrator.acquireLease(mMockProfile, LeaseReason.FOR_TESTING);
        assertNotNull(lease);
        lease.destroy();

        assertEquals(0, mOrchestrator.getLeaseCountForTesting());
        assertNotNull(mOrchestrator.getGracePeriodTeardownRunnableForTesting());

        // Re-acquire before grace period expiration.
        Destroyable lease2 =
                ArchivedTabModelOrchestrator.acquireLease(mMockProfile, LeaseReason.FOR_TESTING);
        assertNotNull(lease2);
        assertEquals(1, mOrchestrator.getLeaseCountForTesting());
        assertNull(mOrchestrator.getGracePeriodTeardownRunnableForTesting());

        lease2.destroy();
        assertEquals(0, mOrchestrator.getLeaseCountForTesting());
        assertNotNull(mOrchestrator.getGracePeriodTeardownRunnableForTesting());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testAcquireLeaseAndTeardownFlagDisabled() {
        ArchivedTabModelOrchestrator.setInstanceForTesting(mOrchestrator);

        Destroyable lease =
                ArchivedTabModelOrchestrator.acquireLease(mMockProfile, LeaseReason.FOR_TESTING);
        assertNotNull(lease);
        assertEquals(0, mOrchestrator.getLeaseCountForTesting());
        assertNull(mOrchestrator.getGracePeriodTeardownRunnableForTesting());

        lease.destroy();
        assertEquals(0, mOrchestrator.getLeaseCountForTesting());
        assertNull(mOrchestrator.getGracePeriodTeardownRunnableForTesting());

        mOrchestrator.markTabModelsInitialized();
        when(mMockTabModelSelector.isTabStateInitialized()).thenReturn(true);
        mOrchestrator.setTabModelSelectorForTesting(mMockTabModelSelector);
        mOrchestrator.performTeardownForTesting();
        verify(mMockTabPersistentStore, never()).saveState();
    }

    @Test
    public void testGetTabContentManager_fallbackToSavedActivityOrchestrators() {
        Profile otherProfile = mock(Profile.class);
        TabbedModeTabModelOrchestrator otherActivityOrchestrator =
                mock(TabbedModeTabModelOrchestrator.class);
        TabContentManager otherManager = mock(TabContentManager.class);
        when(otherManager.isDestroyed()).thenReturn(false);
        when(otherActivityOrchestrator.getOriginalProfile()).thenReturn(otherProfile);
        when(otherActivityOrchestrator.getTabContentManager()).thenReturn(otherManager);

        TabbedModeTabModelOrchestrator matchingActivityOrchestrator =
                mock(TabbedModeTabModelOrchestrator.class);
        TabContentManager matchingManager = mock(TabContentManager.class);
        when(matchingManager.isDestroyed()).thenReturn(false);
        when(matchingActivityOrchestrator.getOriginalProfile()).thenReturn(mMockProfile);
        when(matchingActivityOrchestrator.getTabContentManager()).thenReturn(matchingManager);

        ArchivedTabModelOrchestrator.registerSavedActivityOrchestratorForTesting(
                otherActivityOrchestrator);
        assertEquals(otherManager, mOrchestrator.getTabContentManager());

        ArchivedTabModelOrchestrator.registerSavedActivityOrchestratorForTesting(
                matchingActivityOrchestrator);
        assertEquals(matchingManager, mOrchestrator.getTabContentManager());

        // When matching manager is destroyed, falls back to non-destroyed manager, and returns null
        // when all managers are destroyed.
        when(matchingManager.isDestroyed()).thenReturn(true);
        assertEquals(otherManager, mOrchestrator.getTabContentManager());
        when(otherManager.isDestroyed()).thenReturn(true);
        assertNull(mOrchestrator.getTabContentManager());
    }

    @Test
    public void testTabStateInitializedSupplier() {
        assertFalse(mOrchestrator.isTabStateInitialized());
        assertFalse(mOrchestrator.getTabStateInitializedSupplier().get());

        AtomicBoolean callbackFired = new AtomicBoolean(false);
        mOrchestrator.runOnTabStateInitialized(() -> callbackFired.set(true));
        assertFalse(callbackFired.get());

        when(mMockTabModelSelector.isTabStateInitialized()).thenReturn(true);
        mOrchestrator.setTabModelSelectorForTesting(mMockTabModelSelector);

        assertTrue(mOrchestrator.isTabStateInitialized());
        assertTrue(mOrchestrator.getTabStateInitializedSupplier().get());
        assertTrue(callbackFired.get());

        // Running when already initialized runs synchronously.
        AtomicBoolean secondCallbackFired = new AtomicBoolean(false);
        mOrchestrator.runOnTabStateInitialized(() -> secondCallbackFired.set(true));
        assertTrue(secondCallbackFired.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testTeardownSavesStateOnlyWhenInitialized() {
        // Case 1: Tab state not initialized -> saveState() should not be called.
        assertFalse(mOrchestrator.isTabStateInitialized());
        mOrchestrator.markTabModelsInitialized();
        mOrchestrator.performTeardownForTesting();
        verify(mMockTabPersistentStore, never()).saveState();

        // Recreate for Case 2.
        mOrchestrator = new ArchivedTabModelOrchestrator(mMockProfile);
        mOrchestrator.initForTesting(
                mMockTabModelSelector,
                mMockTabPersistentStore,
                mMockTabPersistencePolicy,
                mMockShadowTabPersistentStore);
        mOrchestrator.markTabModelsInitialized();
        when(mMockTabModelSelector.isTabStateInitialized()).thenReturn(true);
        mOrchestrator.setTabModelSelectorForTesting(mMockTabModelSelector);
        assertTrue(mOrchestrator.isTabStateInitialized());

        mOrchestrator.performTeardownForTesting();
        verify(mMockTabPersistentStore).saveState();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testIsInstantiatedForProfile() {
        assertFalse(ArchivedTabModelOrchestrator.isInstantiatedForProfile(mMockProfile));
        assertNull(ArchivedTabModelOrchestrator.getIfInstantiatedForProfile(mMockProfile));

        ArchivedTabModelOrchestrator orchestrator =
                ArchivedTabModelOrchestrator.getForProfile(mMockProfile);
        assertNotNull(orchestrator);
        assertTrue(ArchivedTabModelOrchestrator.isInstantiatedForProfile(mMockProfile));
        assertEquals(
                orchestrator,
                ArchivedTabModelOrchestrator.getIfInstantiatedForProfile(mMockProfile));

        orchestrator.performTeardownForTesting();
        assertFalse(ArchivedTabModelOrchestrator.isInstantiatedForProfile(mMockProfile));
        assertNull(ArchivedTabModelOrchestrator.getIfInstantiatedForProfile(mMockProfile));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testRescueArchivedTabs_DuplicateCall_CompletesOrchestrator() {
        TabbedModeTabModelOrchestrator mockOrchestrator =
                mock(TabbedModeTabModelOrchestrator.class);

        mOrchestrator.setRescueTabsCalledForTesting(/* called= */ true);
        mOrchestrator.rescueArchivedTabs(mockOrchestrator);

        verify(mockOrchestrator).onRescueArchivedTabsCompleted();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testProfileBoundaryGuards() {
        Profile uninitializedProfile = mock(Profile.class);
        when(uninitializedProfile.isNativeInitialized()).thenReturn(false);

        Profile shutdownProfile = mock(Profile.class);
        when(shutdownProfile.isNativeInitialized()).thenReturn(true);
        when(shutdownProfile.shutdownStarted()).thenReturn(true);

        Profile otrProfile = mock(Profile.class);
        when(otrProfile.isNativeInitialized()).thenReturn(true);
        when(otrProfile.shutdownStarted()).thenReturn(false);
        when(otrProfile.isOffTheRecord()).thenReturn(true);
        when(otrProfile.getOriginalProfile()).thenReturn(mMockProfile);

        assertNull(ArchivedTabModelOrchestrator.acquireLease(null, LeaseReason.FOR_TESTING));
        assertNull(
                ArchivedTabModelOrchestrator.acquireLease(
                        uninitializedProfile, LeaseReason.FOR_TESTING));
        assertNull(
                ArchivedTabModelOrchestrator.acquireLease(
                        shutdownProfile, LeaseReason.FOR_TESTING));
        assertNull(ArchivedTabModelOrchestrator.acquireLease(otrProfile, LeaseReason.FOR_TESTING));

        assertFalse(ArchivedTabModelOrchestrator.isInstantiatedForProfile(null));
        assertFalse(ArchivedTabModelOrchestrator.isInstantiatedForProfile(uninitializedProfile));
        assertFalse(ArchivedTabModelOrchestrator.isInstantiatedForProfile(shutdownProfile));
        assertFalse(ArchivedTabModelOrchestrator.isInstantiatedForProfile(otrProfile));

        assertNull(ArchivedTabModelOrchestrator.getIfInstantiatedForProfile(null));
        assertNull(ArchivedTabModelOrchestrator.getIfInstantiatedForProfile(uninitializedProfile));
        assertNull(ArchivedTabModelOrchestrator.getIfInstantiatedForProfile(shutdownProfile));
        assertNull(ArchivedTabModelOrchestrator.getIfInstantiatedForProfile(otrProfile));

        assertThrows(Throwable.class, () -> ArchivedTabModelOrchestrator.getForProfile(null));
        assertThrows(
                Throwable.class,
                () -> ArchivedTabModelOrchestrator.getForProfile(uninitializedProfile));
        assertThrows(
                Throwable.class, () -> ArchivedTabModelOrchestrator.getForProfile(shutdownProfile));
        assertThrows(
                IllegalStateException.class,
                () -> ArchivedTabModelOrchestrator.getForProfile(otrProfile));
    }

    @Test
    public void testInstanceForTestingBypassesProfileCheck() {
        Profile mockProfile = mock(Profile.class);
        // Note: mockProfile.isNativeInitialized() returns false by default for mocks.
        ArchivedTabModelOrchestrator.setInstanceForTesting(mOrchestrator);

        assertTrue(ArchivedTabModelOrchestrator.isInstantiatedForProfile(mockProfile));
        assertEquals(mOrchestrator, ArchivedTabModelOrchestrator.getForProfile(mockProfile));
        assertEquals(
                mOrchestrator,
                ArchivedTabModelOrchestrator.getIfInstantiatedForProfile(mockProfile));
        Destroyable lease =
                ArchivedTabModelOrchestrator.acquireLease(mockProfile, LeaseReason.FOR_TESTING);
        assertNotNull(lease);
        lease.destroy();
    }

    @Test
    public void testOrchestratorDestroyDoesNotDestroyTabArchiveSettings() {
        TabArchiveSettings settings = TabArchiveSettings.getInstance();
        settings.setArchivedTabCount(5);
        assertEquals(5, settings.getArchivedTabCount());

        mOrchestrator.destroy();

        // Singleton settings and supplier remain intact and functional.
        assertEquals(5, settings.getArchivedTabCount());
        settings.setArchivedTabCount(10);
        assertEquals(10, settings.getArchivedTabCount());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testTeardownMetrics() {
        var timeResidentWatcher =
                HistogramWatcher.newSingleRecordWatcher("Tabs.ArchivedTabs.TimeResidentMs", 5000);

        mFakeTimeTestRule.advanceMillis(5000);
        mOrchestrator.performTeardownForTesting();
        // Subsequent destroy() call should be a no-op and not double-record.
        mOrchestrator.destroy();

        timeResidentWatcher.assertExpected();
    }

    @Test
    public void testOnSettingChanged_WhenOrchestratorsEmpty_DoesNotRescue() {
        TabArchiver mockArchiver = mock(TabArchiver.class);
        mOrchestrator.setTabArchiverForTesting(mockArchiver);
        mOrchestrator.setTabGroupSyncServiceForTesting(mock(TabGroupSyncService.class));

        when(mMockTabModelSelector.isTabStateInitialized()).thenReturn(true);
        mOrchestrator.setTabModelSelectorForTesting(mMockTabModelSelector);

        mOrchestrator.getTabArchiveSettings().setArchiveEnabled(false);
        mOrchestrator.getTabArchiveSettingsObserverForTesting().onSettingChanged();

        verify(mockArchiver, never()).rescueArchivedTabs(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testOnSettingChanged_PreservesRegisteredOrchestratorAcrossTeardown() {
        TabbedModeTabModelOrchestrator mockTabbedOrchestrator =
                mock(TabbedModeTabModelOrchestrator.class);
        TabModelSelectorBase mockSelector = mock(TabModelSelectorBase.class);
        TabCreatorManager mockCreatorManager = mock(TabCreatorManager.class);
        TabCreator mockCreator = mock(TabCreator.class);
        when(mockTabbedOrchestrator.getTabModelSelector()).thenReturn(mockSelector);
        when(mockTabbedOrchestrator.getTabPersistentStore()).thenReturn(mMockTabbedPersistentStore);
        when(mockSelector.isTabStateInitialized()).thenReturn(true);
        when(mockSelector.getTabCreatorManager()).thenReturn(mockCreatorManager);
        when(mockCreatorManager.getTabCreator(/* incognito= */ false)).thenReturn(mockCreator);

        ArchivedTabModelOrchestrator.registerSavedActivityOrchestratorForTesting(
                mockTabbedOrchestrator);
        mOrchestrator.performTeardownForTesting();

        ArchivedTabModelOrchestrator newOrchestrator =
                new ArchivedTabModelOrchestrator(mMockProfile);
        when(mMockTabModelSelector.isTabStateInitialized()).thenReturn(true);
        newOrchestrator.initForTesting(
                mMockTabModelSelector,
                mMockTabPersistentStore,
                mMockTabPersistencePolicy,
                mMockShadowTabPersistentStore);
        TabArchiver mockArchiver = mock(TabArchiver.class);
        newOrchestrator.setTabArchiverForTesting(mockArchiver);
        newOrchestrator.setTabGroupSyncServiceForTesting(mMockTabGroupSyncService);
        newOrchestrator.setArchivedTabCreatorForTesting(mMockArchivedTabCreator);
        newOrchestrator.setWindowAndroidForTesting(mMockWindowAndroid);
        newOrchestrator.setCipherFactoryForTesting(mMockCipherFactory);

        newOrchestrator.getTabArchiveSettings().setArchiveEnabled(false);
        newOrchestrator.getTabArchiveSettingsObserverForTesting().onSettingChanged();

        verify(mockArchiver).rescueArchivedTabs(eq(mockCreator));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testOnSettingChanged_UnregisteredOrchestratorNotPreservedAcrossTeardown() {
        TabbedModeTabModelOrchestrator mockTabbedOrchestrator =
                mock(TabbedModeTabModelOrchestrator.class);
        ArchivedTabModelOrchestrator.registerSavedActivityOrchestratorForTesting(
                mockTabbedOrchestrator);
        ArchivedTabModelOrchestrator.unregisterTabModelOrchestrator(mockTabbedOrchestrator);
        mOrchestrator.performTeardownForTesting();

        assertTrue(
                ArchivedTabModelOrchestrator.getActivityTabModelOrchestratorsForTesting()
                        .isEmpty());
        assertFalse(ArchivedTabModelOrchestrator.isOrchestratorRegistered(mockTabbedOrchestrator));
    }

    @Test
    public void testOnSettingChanged_ArchiveEnabled_DoesNotRescue() {
        TabArchiver mockArchiver = mock(TabArchiver.class);
        mOrchestrator.setTabArchiverForTesting(mockArchiver);
        mOrchestrator.setTabGroupSyncServiceForTesting(mMockTabGroupSyncService);

        when(mMockTabModelSelector.isTabStateInitialized()).thenReturn(true);
        mOrchestrator.setTabModelSelectorForTesting(mMockTabModelSelector);

        mOrchestrator.getTabArchiveSettings().setArchiveEnabled(true);
        mOrchestrator.getTabArchiveSettingsObserverForTesting().onSettingChanged();

        verify(mockArchiver, never()).rescueArchivedTabs(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testRescueArchivedTabs_AcquiresAndReleasesLease() {
        TabbedModeTabModelOrchestrator mockOrchestrator =
                mock(TabbedModeTabModelOrchestrator.class);
        TabModelSelectorBase mockSelector = mock(TabModelSelectorBase.class);
        TabCreatorManager mockCreatorManager = mock(TabCreatorManager.class);
        TabCreator mockCreator = mock(TabCreator.class);
        when(mockOrchestrator.getTabModelSelector()).thenReturn(mockSelector);
        when(mockOrchestrator.getTabPersistentStore()).thenReturn(mMockTabbedPersistentStore);
        when(mockSelector.getTabCreatorManager()).thenReturn(mockCreatorManager);
        when(mockCreatorManager.getTabCreator(/* incognito= */ false)).thenReturn(mockCreator);

        TabArchiver mockArchiver = mock(TabArchiver.class);
        mOrchestrator.setTabArchiverForTesting(mockArchiver);
        mOrchestrator.setTabGroupSyncServiceForTesting(mMockTabGroupSyncService);

        when(mMockTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mockSelector.isTabStateInitialized()).thenReturn(false);

        ArgumentCaptor<TabModelSelectorObserver> observerCaptor =
                ArgumentCaptor.forClass(TabModelSelectorObserver.class);

        mOrchestrator.rescueArchivedTabs(mockOrchestrator);

        assertNotNull(mOrchestrator.getRescueLeaseForTesting());
        assertEquals(1, mOrchestrator.getLeaseCountForTesting());
        verify(mockArchiver, never()).rescueArchivedTabs(any());

        verify(mockSelector).addObserver(observerCaptor.capture());
        observerCaptor.getValue().onTabStateInitialized();

        verify(mockArchiver).rescueArchivedTabs(eq(mockCreator));
        assertNull(mOrchestrator.getRescueLeaseForTesting());
        assertEquals(0, mOrchestrator.getLeaseCountForTesting());
        verify(mockOrchestrator).onRescueArchivedTabsCompleted();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testRescueArchivedTabs_DestroyReleasesLease() {
        TabbedModeTabModelOrchestrator mockOrchestrator =
                mock(TabbedModeTabModelOrchestrator.class);
        TabModelSelectorBase mockSelector = mock(TabModelSelectorBase.class);
        when(mockOrchestrator.getTabModelSelector()).thenReturn(mockSelector);
        when(mMockTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mockSelector.isTabStateInitialized()).thenReturn(false);

        mOrchestrator.rescueArchivedTabs(mockOrchestrator);

        assertNotNull(mOrchestrator.getRescueLeaseForTesting());
        assertEquals(1, mOrchestrator.getLeaseCountForTesting());

        mOrchestrator.destroy();

        assertNull(mOrchestrator.getRescueLeaseForTesting());
        assertEquals(0, mOrchestrator.getLeaseCountForTesting());
        verify(mockOrchestrator).onRescueArchivedTabsCompleted();
    }

    @Test
    public void testSetInstanceForTesting_NullReset() {
        ArchivedTabModelOrchestrator.setInstanceForTesting(mOrchestrator);
        assertTrue(ArchivedTabModelOrchestrator.isInstantiatedForProfile(mMockProfile));
        assertEquals(mOrchestrator, ArchivedTabModelOrchestrator.getForProfile(mMockProfile));

        ArchivedTabModelOrchestrator.setInstanceForTesting(null);
        ArchivedTabModelOrchestrator.destroyProfileKeyedMap();
        assertFalse(ArchivedTabModelOrchestrator.isInstantiatedForProfile(mMockProfile));
    }

    @Test
    public void testDestroy_ClearsObservers() {
        ArchivedTabModelOrchestrator.Observer mockObserver =
                mock(ArchivedTabModelOrchestrator.Observer.class);

        mOrchestrator.addObserver(mockObserver);
        assertFalse(mOrchestrator.getObserversForTesting().isEmpty());

        mOrchestrator.destroy();

        assertTrue(mOrchestrator.getObserversForTesting().isEmpty());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testPerformTeardown_ClearsObservers() {
        ArchivedTabModelOrchestrator.Observer mockObserver =
                mock(ArchivedTabModelOrchestrator.Observer.class);

        mOrchestrator.addObserver(mockObserver);
        assertFalse(mOrchestrator.getObserversForTesting().isEmpty());

        mOrchestrator.performTeardownForTesting();

        assertTrue(mOrchestrator.getObserversForTesting().isEmpty());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testPerformTeardown_EvictsBeforeDestroy() {
        ArchivedTabModelOrchestrator orchestrator =
                ArchivedTabModelOrchestrator.getForProfile(mMockProfile);
        orchestrator.initForTesting(
                mMockTabModelSelector,
                mMockTabPersistentStore,
                mMockTabPersistencePolicy,
                mMockShadowTabPersistentStore);
        WindowAndroid mockWindow = mock(WindowAndroid.class);
        orchestrator.setWindowAndroidForTesting(mockWindow);

        AtomicBoolean destroyCallbackRan = new AtomicBoolean(false);
        doAnswer(
                        inv -> {
                            destroyCallbackRan.set(true);
                            assertFalse(
                                    ArchivedTabModelOrchestrator.isInstantiatedForProfile(
                                            mMockProfile));
                            assertNull(
                                    ArchivedTabModelOrchestrator.getIfInstantiatedForProfile(
                                            mMockProfile));
                            assertNull(ArchivedTabModelSelectorHolder.getInstance(mMockProfile));
                            return null;
                        })
                .when(mockWindow)
                .destroy();

        orchestrator.performTeardownForTesting();
        assertTrue(destroyCallbackRan.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testRescueArchivedTabs_DuplicateCallWhileInFlight_CompletesOnceAfterRescue() {
        TabbedModeTabModelOrchestrator mockOrchestrator =
                mock(TabbedModeTabModelOrchestrator.class);
        TabModelSelectorBase mockSelector = mock(TabModelSelectorBase.class);
        TabCreatorManager mockCreatorManager = mock(TabCreatorManager.class);
        TabCreator mockCreator = mock(TabCreator.class);
        when(mockOrchestrator.getTabModelSelector()).thenReturn(mockSelector);
        when(mockOrchestrator.getTabPersistentStore()).thenReturn(mMockTabbedPersistentStore);
        when(mockSelector.getTabCreatorManager()).thenReturn(mockCreatorManager);
        when(mockCreatorManager.getTabCreator(/* incognito= */ false)).thenReturn(mockCreator);

        TabArchiver mockArchiver = mock(TabArchiver.class);
        mOrchestrator.setTabArchiverForTesting(mockArchiver);
        mOrchestrator.setTabGroupSyncServiceForTesting(mMockTabGroupSyncService);

        when(mMockTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mockSelector.isTabStateInitialized()).thenReturn(false);

        ArgumentCaptor<TabModelSelectorObserver> observerCaptor =
                ArgumentCaptor.forClass(TabModelSelectorObserver.class);

        // First call starts an async rescue waiting on mockSelector tab state initialization.
        mOrchestrator.rescueArchivedTabs(mockOrchestrator);
        // Second call while rescue is still in flight must NOT fire completion prematurely.
        mOrchestrator.rescueArchivedTabs(mockOrchestrator);

        verify(mockOrchestrator, never()).onRescueArchivedTabsCompleted();
        verify(mockArchiver, never()).rescueArchivedTabs(any());

        verify(mockSelector).addObserver(observerCaptor.capture());
        observerCaptor.getValue().onTabStateInitialized();

        verify(mockArchiver).rescueArchivedTabs(eq(mockCreator));
        verify(mockOrchestrator, times(1)).onRescueArchivedTabsCompleted();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testDoDeclutterPass_DestroyedBeforeCompletion_SafeNoOp() {
        TabbedModeTabModelOrchestrator mockOrchestrator =
                mock(TabbedModeTabModelOrchestrator.class);
        TabModelSelectorBase mockSelector = mock(TabModelSelectorBase.class);
        when(mockOrchestrator.getTabModelSelector()).thenReturn(mockSelector);
        when(mockOrchestrator.getTabPersistentStore()).thenReturn(mMockTabbedPersistentStore);
        when(mMockTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mockSelector.isTabStateInitialized()).thenReturn(true);

        TabArchiver mockArchiver = mock(TabArchiver.class);
        mOrchestrator.setTabArchiverForTesting(mockArchiver);
        mOrchestrator.setTabGroupSyncServiceForTesting(mMockTabGroupSyncService);
        mOrchestrator.getTabArchiveSettings().setArchiveEnabled(true);

        ArgumentCaptor<TabArchiver.Observer> observerCaptor =
                ArgumentCaptor.forClass(TabArchiver.Observer.class);

        mOrchestrator.doDeclutterPass(mockOrchestrator);
        verify(mockArchiver).addObserver(observerCaptor.capture());
        verify(mMockTabbedPersistentStore).pauseSaveTabList();
        verify(mMockTabPersistentStore).pauseSaveTabList();

        mOrchestrator.destroy();
        verify(mockArchiver).removeObserver(observerCaptor.getValue());
        verify(mMockTabPersistentStore).resumeSaveTabList();
        verify(mMockTabbedPersistentStore).resumeSaveTabList();
        verify(mockOrchestrator).onDeclutterPassCompleted();

        clearInvocations(mockOrchestrator);
        // Subsequent observer callback is a safe no-op and does not double-complete.
        observerCaptor.getValue().onDeclutterPassCompleted();
        verify(mockOrchestrator, never()).onDeclutterPassCompleted();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ARCHIVED_TABS_TEARDOWN)
    public void testDoDeclutterPass_ConcurrentMultiWindow_DoesNotPrematurelyCompleteFirstWindow() {
        TabbedModeTabModelOrchestrator mockOrchestrator1 =
                mock(TabbedModeTabModelOrchestrator.class);
        TabModelSelectorBase mockSelector1 = mock(TabModelSelectorBase.class);
        TabPersistentStore mockStore1 = mock(TabPersistentStore.class);
        when(mockOrchestrator1.getTabModelSelector()).thenReturn(mockSelector1);
        when(mockOrchestrator1.getTabPersistentStore()).thenReturn(mockStore1);
        when(mockSelector1.isTabStateInitialized()).thenReturn(true);

        TabbedModeTabModelOrchestrator mockOrchestrator2 =
                mock(TabbedModeTabModelOrchestrator.class);
        TabModelSelectorBase mockSelector2 = mock(TabModelSelectorBase.class);
        TabPersistentStore mockStore2 = mock(TabPersistentStore.class);
        when(mockOrchestrator2.getTabModelSelector()).thenReturn(mockSelector2);
        when(mockOrchestrator2.getTabPersistentStore()).thenReturn(mockStore2);
        when(mockSelector2.isTabStateInitialized()).thenReturn(true);

        when(mMockTabModelSelector.isTabStateInitialized()).thenReturn(true);

        TabArchiver mockArchiver = mock(TabArchiver.class);
        mOrchestrator.setTabArchiverForTesting(mockArchiver);
        mOrchestrator.setTabGroupSyncServiceForTesting(mMockTabGroupSyncService);
        mOrchestrator.getTabArchiveSettings().setArchiveEnabled(true);

        ArgumentCaptor<TabArchiver.Observer> observerCaptor =
                ArgumentCaptor.forClass(TabArchiver.Observer.class);

        mOrchestrator.doDeclutterPass(mockOrchestrator1);
        mOrchestrator.doDeclutterPass(mockOrchestrator2);

        verify(mockArchiver, times(2)).addObserver(observerCaptor.capture());
        verify(mockStore1).pauseSaveTabList();
        verify(mockStore2).pauseSaveTabList();

        // Starting the second window's declutter pass while the first is in flight must not
        // prematurely resume the first window's store or complete its declutter pass.
        verify(mockArchiver, never()).removeObserver(any());
        verify(mockStore1, never()).resumeSaveTabList();
        verify(mockOrchestrator1, never()).onDeclutterPassCompleted();

        TabArchiver.Observer observer1 = observerCaptor.getAllValues().get(0);
        TabArchiver.Observer observer2 = observerCaptor.getAllValues().get(1);

        observer1.onDeclutterPassCompleted();
        verify(mockArchiver).removeObserver(observer1);
        verify(mockStore1).resumeSaveTabList();
        verify(mockOrchestrator1).onDeclutterPassCompleted();
        verify(mockStore2, never()).resumeSaveTabList();
        verify(mockOrchestrator2, never()).onDeclutterPassCompleted();

        observer2.onDeclutterPassCompleted();
        verify(mockArchiver).removeObserver(observer2);
        verify(mockStore2).resumeSaveTabList();
        verify(mockOrchestrator2).onDeclutterPassCompleted();
    }
}
