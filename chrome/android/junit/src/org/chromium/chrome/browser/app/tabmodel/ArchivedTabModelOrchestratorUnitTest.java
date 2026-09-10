// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator.LeaseReason;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link ArchivedTabModelOrchestrator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ArchivedTabModelOrchestratorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabPersistentStore mMockTabPersistentStore;
    @Mock private TabPersistentStore mMockShadowTabPersistentStore;
    @Mock private TabModelSelectorBase mMockTabModelSelector;
    @Mock private TabPersistencePolicy mMockTabPersistencePolicy;
    @Mock private Profile mMockProfile;

    private ArchivedTabModelOrchestrator mOrchestrator;

    @Before
    public void setUp() {
        when(mMockProfile.getOriginalProfile()).thenReturn(mMockProfile);
        mOrchestrator = new ArchivedTabModelOrchestrator(mMockProfile);
        mOrchestrator.initForTesting(
                mMockTabModelSelector,
                mMockTabPersistentStore,
                mMockTabPersistencePolicy,
                mMockShadowTabPersistentStore);
    }

    @After
    public void tearDown() {
        ArchivedTabModelOrchestrator.destroyProfileKeyedMap();
        ArchivedTabModelOrchestrator.setInstanceForTesting(null);
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

        ArchivedTabModelOrchestrator orchestrator =
                ArchivedTabModelOrchestrator.getForProfile(mMockProfile);
        assertNotNull(orchestrator);
        assertTrue(ArchivedTabModelOrchestrator.isInstantiatedForProfile(mMockProfile));

        orchestrator.performTeardownForTesting();
        assertFalse(ArchivedTabModelOrchestrator.isInstantiatedForProfile(mMockProfile));
    }
}
