// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.SessionStartupPolicy;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link TabbedStartupWindowPolicyDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.ON_STARTUP_WINDOW_POLICY,
    ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH,
    ChromeFeatureList.SYNC_RESTORE_ON_STARTUP_PREF
})
public class TabbedStartupWindowPolicyDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityManager mActivityManager;
    @Mock private ChromeTabbedActivity mTabbedActivity;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private SyncService mSyncService;
    @Mock private PrefChangeRegistrar.Natives mMockPrefChangeRegistrarNatives;
    @Mock private TabbedStartupWindowPolicyDelegate.Natives mMockDelegateNatives;

    private TabbedStartupWindowPolicyDelegate mDelegate;

    @Before
    public void setUp() {
        TabbedStartupWindowPolicyDelegate.setInstanceForTesting(null);
        TabbedStartupWindowPolicyDelegateJni.setInstanceForTesting(mMockDelegateNatives);
        when(mMockDelegateNatives.getSessionStartupUrls(any())).thenReturn(List.of());
        PrefChangeRegistrarJni.setInstanceForTesting(mMockPrefChangeRegistrarNatives);
        when(mMockPrefChangeRegistrarNatives.init(any(), any())).thenReturn(117L);
        when(mSyncService.getAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.HISTORY));
        UserPrefs.setPrefServiceForTesting(mPrefService);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        ChromeMultiInstancePersistentStore.ensureInitialized();
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        DeviceInfo.setIsDesktopForTesting(true);
        doReturn(mActivityManager).when(mTabbedActivity).getSystemService(Context.ACTIVITY_SERVICE);
        doReturn(ContextUtils.getApplicationContext().getPackageName())
                .when(mTabbedActivity)
                .getPackageName();
        mDelegate = TabbedStartupWindowPolicyDelegate.getInstance();
    }

    @After
    public void tearDown() {
        mDelegate.resetForTesting();
        ChromeMultiInstancePersistentStore.resetForTesting();
        TabbedStartupWindowPolicyDelegate.setInstanceForTesting(null);
        UserPrefs.setPrefServiceForTesting(null);
        SyncServiceFactory.setInstanceForTesting(null);
    }

    @Test
    public void testApplyPolicy_restoreAll_restoresWindows() {
        // Setup.
        setupRecoverableInstances(SessionStartupPolicy.RESTORE_ALL);

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity).startActivity(any());
        assertEquals(
                SessionStartupPolicy.DEFAULT,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
        assertFalse(
                "isRecoverable should be cleared when restoring window on launch after quit.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testApplyPolicy_defaultPolicy_doesNotRestoreWindows() {
        // Setup.
        setupRecoverableInstances(SessionStartupPolicy.DEFAULT);

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity, never()).startActivity(any());
        assertTrue(
                "isRecoverable should remain true when startup policy is default.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ON_STARTUP_WINDOW_POLICY)
    public void testApplyPolicy_featureDisabled_doesNotRestoreWindows() {
        // Setup.
        setupRecoverableInstances(SessionStartupPolicy.RESTORE_ALL);

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity, never()).startActivity(any());
        assertEquals(
                SessionStartupPolicy.RESTORE_ALL,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    public void testApplyPolicy_restoreAll_aliveTaskNonMultiWindowMode_doesNotRestoreWindow() {
        // Setup.
        setupRecoverableInstances(SessionStartupPolicy.RESTORE_ALL);
        setupAppTasks(1);
        doReturn(false).when(mTabbedActivity).isInMultiWindowMode();

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity, never()).startActivity(any());
        assertTrue(
                "isRecoverable should remain true when task is alive in non-multiwindow mode.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testApplyPolicy_restoreAll_aliveTaskMultiWindowMode_restoresWindow() {
        // Setup.
        setupRecoverableInstances(SessionStartupPolicy.RESTORE_ALL);
        List<AppTask> appTasks = setupAppTasks(1);
        doReturn(true).when(mTabbedActivity).isInMultiWindowMode();

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify.
        verify(appTasks.get(0)).finishAndRemoveTask();
        verify(mTabbedActivity).startActivity(any());
        assertFalse(
                "isRecoverable should be cleared when restoring window in multi-window mode.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testApplyPolicy_restoreAll_killedTask_restoresWindow() {
        // Setup.
        setupRecoverableInstances(SessionStartupPolicy.RESTORE_ALL);
        // Do not add app task ID 1 so it is treated as killed/not alive.
        doReturn(false).when(mTabbedActivity).isInMultiWindowMode();

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity).startActivity(any());
        assertFalse(
                "isRecoverable should be cleared when restoring a killed task.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testApplyPolicy_restoreAll_notRecoverable_doesNotRestoreWindow() {
        // Setup.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 0, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 0);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        ChromeMultiInstancePersistentStore.writeIsRecoverable(1, false);
        ChromeMultiInstancePersistentStore.writeSessionStartupPolicy(
                SessionStartupPolicy.RESTORE_ALL);
        doReturn(0).when(mTabbedActivity).getWindowId();

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity, never()).startActivity(any());
        assertFalse(
                "isRecoverable should remain false for unrecoverable window.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testApplyPolicy_createNew_clearsStartupPolicy() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeSessionStartupPolicy(
                SessionStartupPolicy.CREATE_NEW);

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify.
        assertEquals(
                SessionStartupPolicy.DEFAULT,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    public void testApplyPolicy_incognitoWindow_clearsStartupPolicyWithoutRestoration() {
        // Setup.
        setupRecoverableInstances(SessionStartupPolicy.RESTORE_ALL);
        doReturn(true).when(mTabbedActivity).isIncognitoWindow();

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify: Window restoration is skipped on the incognito host, and startup policy is
        // cleared.
        verify(mTabbedActivity, never()).startActivity(any());
        assertEquals(
                SessionStartupPolicy.DEFAULT,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
        assertTrue(
                "isRecoverable should remain true when incognito window initializes.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testMaybeSaveSessionStateOnTermination_multipleActiveInstances_savesState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);

        // Act.
        mDelegate.maybeSaveSessionStateOnTermination(SessionStartupPolicy.RESTORE_ALL);

        // Verify startup policy is updated.
        assertEquals(
                SessionStartupPolicy.RESTORE_ALL,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    public void testMaybeSaveSessionStateOnTermination_singleActiveInstance_doesNotSaveState() {
        // Setup only 1 active instance.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);

        // Act.
        mDelegate.maybeSaveSessionStateOnTermination(SessionStartupPolicy.RESTORE_ALL);

        // Verify startup policy is not updated.
        assertEquals(
                SessionStartupPolicy.DEFAULT,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    public void testMaybeSaveSessionStateOnTermination_singleActiveInstance_createNew_savesState() {
        // Setup only 1 active instance.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);

        // Act.
        mDelegate.maybeSaveSessionStateOnTermination(SessionStartupPolicy.CREATE_NEW);

        // Verify startup policy is updated even for single instance.
        assertEquals(
                SessionStartupPolicy.CREATE_NEW,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ON_STARTUP_WINDOW_POLICY)
    public void testMaybeSaveSessionStateOnTermination_featureDisabled_doesNotSaveState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);

        // Act.
        mDelegate.maybeSaveSessionStateOnTermination(SessionStartupPolicy.RESTORE_ALL);

        // Verify startup policy is not updated when feature is disabled.
        assertEquals(
                SessionStartupPolicy.DEFAULT,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    public void testMaybeSaveSessionStateOnTermination_startupPrefIsNewTab_doesNotSaveState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(
                SessionStartupPref.NEW_TAB);

        // Act.
        mDelegate.maybeSaveSessionStateOnTermination(SessionStartupPolicy.RESTORE_ALL);

        // Verify startup policy is not updated when startup pref is NEW_TAB.
        assertEquals(
                SessionStartupPolicy.DEFAULT,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    public void testMaybeSaveSessionStateOnTermination_startupPrefIsUrls_doesNotSaveState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.URLS);

        // Act.
        mDelegate.maybeSaveSessionStateOnTermination(SessionStartupPolicy.CREATE_NEW);

        // Verify startup policy is not updated when startup pref is URLS.
        assertEquals(
                SessionStartupPolicy.DEFAULT,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    public void testMaybeSaveSessionStateOnTermination_startupPrefIsLast_savesState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.LAST);

        // Act.
        mDelegate.maybeSaveSessionStateOnTermination(SessionStartupPolicy.RESTORE_ALL);

        // Verify startup policy is updated when startup pref is LAST.
        assertEquals(
                SessionStartupPolicy.RESTORE_ALL,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    public void testMaybeSaveSessionStateOnTermination_startupPrefIsUnset_savesState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(
                TabbedStartupWindowPolicyDelegate.PREF_UNSET);

        // Act.
        mDelegate.maybeSaveSessionStateOnTermination(SessionStartupPolicy.RESTORE_ALL);

        // Verify startup policy is updated when startup pref is UNSET.
        assertEquals(
                SessionStartupPolicy.RESTORE_ALL,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    public void testPreferenceChange_historySyncActive_syncsToCache() {
        // Setup mock native preferences.
        when(mPrefService.getInteger(Pref.RESTORE_ON_STARTUP))
                .thenReturn(SessionStartupPref.NEW_TAB);
        when(mMockDelegateNatives.getSessionStartupUrls(mPrefService))
                .thenReturn(List.of("https://www.google.com"));

        // Act.
        mDelegate.initializeWithNative(mProfile);

        // Verify.
        assertEquals(
                SessionStartupPref.NEW_TAB,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());
        assertEquals(
                List.of("https://www.google.com"),
                ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls());
    }

    @Test
    public void testPreferenceChange_emptyUrls_storesEmptyList() {
        // Setup mock native preferences with empty URLs list.
        when(mPrefService.getInteger(Pref.RESTORE_ON_STARTUP))
                .thenReturn(SessionStartupPref.NEW_TAB);
        when(mMockDelegateNatives.getSessionStartupUrls(mPrefService)).thenReturn(List.of());

        // Act.
        mDelegate.initializeWithNative(mProfile);

        // Verify.
        assertTrue(ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls().isEmpty());
    }

    @Test
    public void testPreferenceChange_historySyncDisabled_storesPrefUnset() {
        // Setup mock native preferences with History sync disabled.
        when(mPrefService.getInteger(Pref.RESTORE_ON_STARTUP))
                .thenReturn(SessionStartupPref.NEW_TAB);
        when(mMockDelegateNatives.getSessionStartupUrls(mPrefService))
                .thenReturn(List.of("https://www.google.com"));
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of());

        // Act.
        mDelegate.initializeWithNative(mProfile);

        // Verify that persistent store returns UNSET and empty URLs.
        assertEquals(
                TabbedStartupWindowPolicyDelegate.PREF_UNSET,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());
        assertTrue(ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls().isEmpty());
    }

    @Test
    public void testPreferenceChange_signedOut_storesPrefUnset() {
        // Setup mock native preferences when user is signed out.
        when(mPrefService.getInteger(Pref.RESTORE_ON_STARTUP))
                .thenReturn(SessionStartupPref.NEW_TAB);
        when(mMockDelegateNatives.getSessionStartupUrls(mPrefService))
                .thenReturn(List.of("https://www.google.com"));
        when(mSyncService.getAccountInfo()).thenReturn(null);

        // Act.
        mDelegate.initializeWithNative(mProfile);

        // Verify that persistent store returns UNSET and empty URLs.
        assertEquals(
                TabbedStartupWindowPolicyDelegate.PREF_UNSET,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());
        assertTrue(ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls().isEmpty());
    }

    @Test
    public void testPreferenceChange_syncServiceNull_storesPrefUnset() {
        // Setup mock native preferences when SyncService is null.
        when(mPrefService.getInteger(Pref.RESTORE_ON_STARTUP))
                .thenReturn(SessionStartupPref.NEW_TAB);
        when(mMockDelegateNatives.getSessionStartupUrls(mPrefService))
                .thenReturn(List.of("https://www.google.com"));
        SyncServiceFactory.setInstanceForTesting(null);

        // Act.
        mDelegate.initializeWithNative(mProfile);

        // Verify that persistent store returns UNSET and empty URLs.
        assertEquals(
                TabbedStartupWindowPolicyDelegate.PREF_UNSET,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());
        assertTrue(ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls().isEmpty());
    }

    @Test
    public void testSyncStateChanged_historySyncToggledOff_clearsCache() {
        // Setup initially active History sync with cached preferences.
        when(mPrefService.getInteger(Pref.RESTORE_ON_STARTUP))
                .thenReturn(SessionStartupPref.NEW_TAB);
        when(mMockDelegateNatives.getSessionStartupUrls(mPrefService))
                .thenReturn(List.of("https://www.google.com"));
        mDelegate.initializeWithNative(mProfile);
        assertEquals(
                SessionStartupPref.NEW_TAB,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());

        // Act: Toggle History sync off.
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of());
        mDelegate.syncStateChanged();

        // Verify that persistent store is reset to UNSET and empty URLs.
        assertEquals(
                TabbedStartupWindowPolicyDelegate.PREF_UNSET,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());
        assertTrue(ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls().isEmpty());
    }

    @Test
    public void testSyncStateChanged_signedOut_clearsCache() {
        // Setup initially active History sync with cached preferences.
        when(mPrefService.getInteger(Pref.RESTORE_ON_STARTUP))
                .thenReturn(SessionStartupPref.NEW_TAB);
        when(mMockDelegateNatives.getSessionStartupUrls(mPrefService))
                .thenReturn(List.of("https://www.google.com"));
        mDelegate.initializeWithNative(mProfile);
        assertEquals(
                SessionStartupPref.NEW_TAB,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());

        // Act: Sign out.
        when(mSyncService.getAccountInfo()).thenReturn(null);
        mDelegate.syncStateChanged();

        // Verify that persistent store is reset to UNSET and empty URLs.
        assertEquals(
                TabbedStartupWindowPolicyDelegate.PREF_UNSET,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());
        assertTrue(ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls().isEmpty());
    }

    @Test
    public void testSyncStateChanged_historySyncToggledOn_syncsToCache() {
        // Setup initially inactive History sync.
        when(mPrefService.getInteger(Pref.RESTORE_ON_STARTUP))
                .thenReturn(SessionStartupPref.NEW_TAB);
        when(mMockDelegateNatives.getSessionStartupUrls(mPrefService))
                .thenReturn(List.of("https://www.google.com"));
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of());
        mDelegate.initializeWithNative(mProfile);
        assertEquals(
                TabbedStartupWindowPolicyDelegate.PREF_UNSET,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());

        // Act: Toggle History sync on.
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.HISTORY));
        mDelegate.syncStateChanged();

        // Verify that persistent store now caches synced preferences.
        assertEquals(
                SessionStartupPref.NEW_TAB,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());
        assertEquals(
                List.of("https://www.google.com"),
                ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SYNC_RESTORE_ON_STARTUP_PREF)
    public void testPreferenceChange_featureDisabled_doesNotSyncOrInitialize() {
        // Setup mock native preferences.
        when(mPrefService.getInteger(Pref.RESTORE_ON_STARTUP))
                .thenReturn(SessionStartupPref.NEW_TAB);

        // Act.
        mDelegate.initializeWithNative(mProfile);

        // Verify that persistent store returns default values.
        assertEquals(
                TabbedStartupWindowPolicyDelegate.PREF_UNSET,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());
        assertTrue(ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls().isEmpty());

        // Verify that we never register preference observer.
        verify(mMockPrefChangeRegistrarNatives, never()).init(any(), any());
        verify(mMockDelegateNatives, never()).getSessionStartupUrls(any());
        verify(mSyncService, never()).addSyncStateChangedListener(any());
    }

    @Test
    public void testClaimForceNewInstancePolicy_createNew_startupPrefIsUnset_returnsTrue() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeSessionStartupPolicy(
                SessionStartupPolicy.CREATE_NEW);

        // Act & Verify.
        assertTrue(mDelegate.claimForceNewInstancePolicy(false));
        // Verify startup policy is preserved until onTabbedActivityInitialized is called.
        assertEquals(
                SessionStartupPolicy.CREATE_NEW,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ON_STARTUP_WINDOW_POLICY)
    public void testClaimForceNewInstancePolicy_createNew_featureDisabled_returnsFalse() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeSessionStartupPolicy(
                SessionStartupPolicy.CREATE_NEW);

        // Act & Verify.
        assertFalse(mDelegate.claimForceNewInstancePolicy(false));
    }

    @Test
    public void testClaimForceNewInstancePolicy_newTab_returnsTrueOnce() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(
                SessionStartupPref.NEW_TAB);

        // Act & Verify.
        // First allocation in browser process should honor NEW_TAB startup policy.
        assertTrue(mDelegate.claimForceNewInstancePolicy(false));

        // Subsequent allocations in the same browser process should not honor startup policy.
        assertFalse(mDelegate.claimForceNewInstancePolicy(false));
    }

    @Test
    public void testClaimForceNewInstancePolicy_isIncognito_returnsFalseAndClaimsPolicy() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(
                SessionStartupPref.NEW_TAB);

        // Act & Verify: Incognito returns false.
        assertFalse(mDelegate.claimForceNewInstancePolicy(true));

        // Subsequent allocations in the same browser process should observe claimed policy.
        assertFalse(mDelegate.claimForceNewInstancePolicy(false));
    }

    @Test
    public void testClaimForceNewInstancePolicy_otherPolicy_returnsFalse() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.LAST);

        // Act & Verify.
        assertFalse(mDelegate.claimForceNewInstancePolicy(false));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SYNC_RESTORE_ON_STARTUP_PREF)
    public void testClaimForceNewInstancePolicy_restoreOnStartupFeatureDisabled_returnsFalse() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(
                SessionStartupPref.NEW_TAB);

        // Act & Verify.
        assertFalse(mDelegate.claimForceNewInstancePolicy(false));
    }

    @Test
    public void testClaimForceNewInstancePolicy_createNew_startupPrefIsLast_returnsTrue() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeSessionStartupPolicy(
                SessionStartupPolicy.CREATE_NEW);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.LAST);

        // Act & Verify.
        assertTrue(mDelegate.claimForceNewInstancePolicy(false));
        assertEquals(
                SessionStartupPolicy.CREATE_NEW,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    public void testClaimForceNewInstancePolicy_createNew_startupPrefIsUrls_returnsTrue() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeSessionStartupPolicy(
                SessionStartupPolicy.CREATE_NEW);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.URLS);

        // Act & Verify.
        assertTrue(mDelegate.claimForceNewInstancePolicy(false));
        assertEquals(
                SessionStartupPolicy.CREATE_NEW,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
    }

    @Test
    public void testClaimForceNewInstancePolicy_createNew_startupPrefIsNewTab_returnsTrue() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeSessionStartupPolicy(
                SessionStartupPolicy.CREATE_NEW);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(
                SessionStartupPref.NEW_TAB);

        // Act & Verify.
        assertTrue(mDelegate.claimForceNewInstancePolicy(false));
        assertEquals(
                SessionStartupPolicy.CREATE_NEW,
                ChromeMultiInstancePersistentStore.readSessionStartupPolicy());
        assertTrue(mDelegate.resolveStartupUrls(false).isEmpty());
    }

    @Test
    public void testResolveStartupUrls_urls_resolvesOnceAndReturnsConfiguredUrls() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.URLS);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupUrls(
                List.of("https://www.google.com", "https://www.chromium.org"));

        // Act & Verify.
        assertEquals(
                List.of("https://www.google.com", "https://www.chromium.org"),
                mDelegate.resolveStartupUrls(false));

        // Subsequent invocations in the same browser process should return empty list.
        assertTrue(mDelegate.resolveStartupUrls(false).isEmpty());
    }

    @Test
    public void testResolveStartupUrls_urlsEmpty_returnsEmptyList() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.URLS);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupUrls(Collections.emptyList());

        // Act & Verify.
        assertTrue(mDelegate.resolveStartupUrls(false).isEmpty());
    }

    @Test
    public void testResolveStartupUrls_newTab_returnsEmptyList() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(
                SessionStartupPref.NEW_TAB);

        // Act & Verify.
        assertTrue(mDelegate.resolveStartupUrls(false).isEmpty());
    }

    @Test
    public void testResolveStartupUrls_otherPolicy_returnsEmptyList() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.LAST);

        // Act & Verify.
        assertTrue(mDelegate.resolveStartupUrls(false).isEmpty());
    }

    @Test
    public void testResolveStartupUrls_isIncognito_returnsEmptyList() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.URLS);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupUrls(
                List.of("https://www.google.com"));

        // Act & Verify.
        assertTrue(mDelegate.resolveStartupUrls(true).isEmpty());
        // Subsequent calls for regular windows in the same process should also return an empty
        // list.
        assertTrue(mDelegate.resolveStartupUrls(false).isEmpty());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SYNC_RESTORE_ON_STARTUP_PREF)
    public void testResolveStartupUrls_featureDisabled_returnsEmptyList() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.URLS);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupUrls(
                List.of("https://www.google.com"));

        // Act & Verify.
        assertTrue(mDelegate.resolveStartupUrls(false).isEmpty());
    }

    @Test
    public void testResetPolicy_resetsStartupPolicyClaimed() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(
                SessionStartupPref.NEW_TAB);
        assertTrue(mDelegate.claimForceNewInstancePolicy(false));
        assertFalse(mDelegate.claimForceNewInstancePolicy(false));

        // Act.
        mDelegate.resetPolicy();

        // Verify.
        assertTrue(mDelegate.claimForceNewInstancePolicy(false));
    }

    private void setupRecoverableInstances(@SessionStartupPolicy int startupPolicy) {
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 0, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 0);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        ChromeMultiInstancePersistentStore.writeSessionStartupPolicy(startupPolicy);

        doReturn(0).when(mTabbedActivity).getWindowId();
    }

    private List<AppTask> setupAppTasks(Integer... taskIds) {
        List<AppTask> appTasks = new ArrayList<>();
        for (int taskId : taskIds) {
            var appTask = mock(AppTask.class);
            var appTaskInfo = mock(RecentTaskInfo.class);
            appTaskInfo.taskId = taskId;
            when(appTask.getTaskInfo()).thenReturn(appTaskInfo);
            appTasks.add(appTask);
        }
        doReturn(appTasks).when(mActivityManager).getAppTasks();
        return appTasks;
    }
}
