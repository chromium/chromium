// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.TriState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.Collections;
import java.util.Set;

/** Unit tests for {@link NtpBackgroundDataSyncController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpBackgroundDataSyncControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private SyncService mSyncService;
    @Mock private NtpBackgroundDataManager mBackgroundDataManager;

    private NtpBackgroundDataSyncController mController;

    @Before
    public void setUp() {
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        mController = new NtpBackgroundDataSyncController();
        mController.setBackgroundDataManagerForTesting(mBackgroundDataManager);
    }

    @Test
    public void testSingletonInstance() {
        NtpBackgroundDataSyncController instance1 = NtpBackgroundDataSyncController.getInstance();
        NtpBackgroundDataSyncController instance2 = NtpBackgroundDataSyncController.getInstance();
        assertSame(instance1, instance2);

        NtpBackgroundDataSyncController testInstance = new NtpBackgroundDataSyncController();
        NtpBackgroundDataSyncController.setInstanceForTesting(testInstance);
        assertSame(testInstance, NtpBackgroundDataSyncController.getInstance());
    }

    @Test
    public void testInitializationRegistersObservers() {
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.THEMES));

        mController.onFinishNativeInitialization(mProfile);

        verify(mSyncService).addSyncStateChangedListener(mController);
        assertTrue(mController.isThemeSyncEnabled());
        assertEquals(TriState.TRUE, mController.getIsThemeSyncEnabledForTesting());
    }

    @Test
    public void testDestroyRemovesObservers() {
        mController.onFinishNativeInitialization(mProfile);
        mController.destroy();

        verify(mSyncService).removeSyncStateChangedListener(mController);
        assertEquals(TriState.NOT_SET, mController.getIsThemeSyncEnabledForTesting());
    }

    @Test
    public void testThemeSyncEnabledTransition() {
        testThemeSyncTransitionImpl(
                /* initialThemesSelected= */ false,
                /* newThemesSelected= */ true,
                /* expectedEnabled= */ TriState.TRUE);
    }

    @Test
    public void testThemeSyncDisabledTransition() {
        testThemeSyncTransitionImpl(
                /* initialThemesSelected= */ true,
                /* newThemesSelected= */ false,
                /* expectedEnabled= */ TriState.FALSE);
    }

    private void testThemeSyncTransitionImpl(
            boolean initialThemesSelected,
            boolean newThemesSelected,
            @TriState int expectedEnabled) {
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.getSelectedTypes())
                .thenReturn(
                        initialThemesSelected
                                ? Set.of(UserSelectableType.THEMES)
                                : Collections.emptySet());

        mController.onFinishNativeInitialization(mProfile);

        when(mSyncService.getSelectedTypes())
                .thenReturn(
                        newThemesSelected
                                ? Set.of(UserSelectableType.THEMES)
                                : Collections.emptySet());

        mController.syncStateChanged();

        assertEquals(expectedEnabled, mController.getIsThemeSyncEnabledForTesting());
        if (expectedEnabled == TriState.FALSE) {
            verify(mBackgroundDataManager).removeAllNonAndroidPlatformData();
        }
    }

    @Test
    public void testIsThemeSyncEnabledWhenEngineNotInitialized() {
        when(mSyncService.isEngineInitialized()).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.THEMES));

        mController.onFinishNativeInitialization(mProfile);
        assertFalse(mController.isThemeSyncEnabled());
    }
}
