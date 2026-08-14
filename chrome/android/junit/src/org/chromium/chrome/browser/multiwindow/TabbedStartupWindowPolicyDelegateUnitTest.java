// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
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
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.LastSessionExitType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabbedStartupWindowPolicyDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.ON_STARTUP_WINDOW_POLICY,
    ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH,
    ChromeFeatureList.SYNC_RESTORE_ON_STARTUP_PREF
})
public class TabbedStartupWindowPolicyDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityManager mActivityManager;
    @Mock private ChromeTabbedActivity mTabbedActivity;
    @Mock private PrefService mPrefService;
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
    }

    @Test
    public void testApplyPolicy_quit_restoresWindows() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.QUIT);

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity).startActivity(any());
        assertEquals(
                LastSessionExitType.DEFAULT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
        assertFalse(
                "isRecoverable should be cleared when restoring window on launch after quit.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testApplyPolicy_defaultExitType_doesNotRestoreWindows() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.DEFAULT);

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity, never()).startActivity(any());
        assertTrue(
                "isRecoverable should remain true when exit type is default.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ON_STARTUP_WINDOW_POLICY)
    public void testApplyPolicy_featureDisabled_doesNotRestoreWindows() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.QUIT);

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity, never()).startActivity(any());
        assertEquals(
                LastSessionExitType.QUIT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void testApplyPolicy_quit_aliveTaskNonMultiWindowMode_doesNotRestoreWindow() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.QUIT);
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
    public void testApplyPolicy_quit_aliveTaskMultiWindowMode_restoresWindow() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.QUIT);
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
    public void testApplyPolicy_quit_killedTask_restoresWindow() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.QUIT);
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
    public void testApplyPolicy_quit_notRecoverable_doesNotRestoreWindow() {
        // Setup.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 0, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 0);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        ChromeMultiInstancePersistentStore.writeIsRecoverable(1, false);
        ChromeMultiInstancePersistentStore.writeLastSessionExitType(LastSessionExitType.QUIT);
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
    public void testApplyPolicy_lastWindowClosedByApp_clearsExitType() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeLastSessionExitType(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP);

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify.
        assertEquals(
                LastSessionExitType.DEFAULT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void testApplyPolicy_incognitoWindow_clearsExitTypeWithoutRestoration() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.QUIT);
        doReturn(true).when(mTabbedActivity).isIncognitoWindow();

        // Act.
        mDelegate.applyPolicy(mTabbedActivity);

        // Verify: Window restoration is skipped on the incognito host, and exit type is cleared.
        verify(mTabbedActivity, never()).startActivity(any());
        assertEquals(
                LastSessionExitType.DEFAULT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
        assertTrue(
                "isRecoverable should remain true when incognito window initializes.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testMaybeSaveWindowStateOnSessionTermination_multipleActiveInstances_savesState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);

        // Act.
        mDelegate.maybeSaveWindowStateOnSessionTermination(LastSessionExitType.QUIT);

        // Verify exit type is updated.
        assertEquals(
                LastSessionExitType.QUIT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void
            testMaybeSaveWindowStateOnSessionTermination_singleActiveInstance_doesNotSaveState() {
        // Setup only 1 active instance.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);

        // Act.
        mDelegate.maybeSaveWindowStateOnSessionTermination(LastSessionExitType.QUIT);

        // Verify exit type is not updated.
        assertEquals(
                LastSessionExitType.DEFAULT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void
            testMaybeSaveWindowStateOnSessionTermination_singleActiveInstance_closedByApp_savesState() {
        // Setup only 1 active instance.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);

        // Act.
        mDelegate.maybeSaveWindowStateOnSessionTermination(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP);

        // Verify exit type is updated even for single instance.
        assertEquals(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ON_STARTUP_WINDOW_POLICY)
    public void testMaybeSaveWindowStateOnSessionTermination_featureDisabled_doesNotSaveState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);

        // Act.
        mDelegate.maybeSaveWindowStateOnSessionTermination(LastSessionExitType.QUIT);

        // Verify exit type is not updated when feature is disabled.
        assertEquals(
                LastSessionExitType.DEFAULT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void
            testMaybeSaveWindowStateOnSessionTermination_quit_startupPrefIsNewTab_doesNotSaveState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(
                SessionStartupPref.NEW_TAB);

        // Act.
        mDelegate.maybeSaveWindowStateOnSessionTermination(LastSessionExitType.QUIT);

        // Verify exit type is not updated when startup pref is NEW_TAB.
        assertEquals(
                LastSessionExitType.DEFAULT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void
            testMaybeSaveWindowStateOnSessionTermination_quit_startupPrefIsUrls_doesNotSaveState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.URLS);

        // Act.
        mDelegate.maybeSaveWindowStateOnSessionTermination(LastSessionExitType.QUIT);

        // Verify exit type is not updated when startup pref is URLS.
        assertEquals(
                LastSessionExitType.DEFAULT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void testMaybeSaveWindowStateOnSessionTermination_quit_startupPrefIsLast_savesState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.LAST);

        // Act.
        mDelegate.maybeSaveWindowStateOnSessionTermination(LastSessionExitType.QUIT);

        // Verify exit type is updated when startup pref is LAST.
        assertEquals(
                LastSessionExitType.QUIT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void testMaybeSaveWindowStateOnSessionTermination_quit_startupPrefIsUnset_savesState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(
                TabbedStartupWindowPolicyDelegate.PREF_UNSET);

        // Act.
        mDelegate.maybeSaveWindowStateOnSessionTermination(LastSessionExitType.QUIT);

        // Verify exit type is updated when startup pref is UNSET.
        assertEquals(
                LastSessionExitType.QUIT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void testPreferenceChange_syncsToCache() {
        // Setup mock native preferences.
        when(mPrefService.getInteger(Pref.RESTORE_ON_STARTUP))
                .thenReturn(SessionStartupPref.NEW_TAB);
        when(mMockDelegateNatives.getSessionStartupUrls(mPrefService))
                .thenReturn(List.of("https://www.google.com"));

        // Act.
        mDelegate.initializeWithNative(mPrefService);

        // Verify.
        assertEquals(
                SessionStartupPref.NEW_TAB,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());
        assertEquals(
                List.of("https://www.google.com"),
                ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls());
    }

    @Test
    public void testPreferenceChange_emptyUrls_storesNull() {
        // Setup mock native preferences with empty URLs list.
        when(mPrefService.getInteger(Pref.RESTORE_ON_STARTUP))
                .thenReturn(SessionStartupPref.NEW_TAB);
        when(mMockDelegateNatives.getSessionStartupUrls(mPrefService)).thenReturn(List.of());

        // Act.
        mDelegate.initializeWithNative(mPrefService);

        // Verify.
        assertNull(ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SYNC_RESTORE_ON_STARTUP_PREF)
    public void testPreferenceChange_featureDisabled_doesNotSyncOrInitialize() {
        // Setup mock native preferences.
        when(mPrefService.getInteger(Pref.RESTORE_ON_STARTUP))
                .thenReturn(SessionStartupPref.NEW_TAB);

        // Act.
        mDelegate.initializeWithNative(mPrefService);

        // Verify that persistent store returns default values.
        assertEquals(
                TabbedStartupWindowPolicyDelegate.PREF_UNSET,
                ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue());
        assertNull(ChromeMultiInstancePersistentStore.readRestoreOnStartupUrls());

        // Verify that we never register preference observer.
        verify(mMockPrefChangeRegistrarNatives, never()).init(any(), any());
        verify(mMockDelegateNatives, never()).getSessionStartupUrls(any());
    }

    @Test
    public void
            testClaimForceNewInstancePolicy_lastWindowClosedByApp_startupPrefIsUnset_returnsTrue() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeLastSessionExitType(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP);

        // Act & Verify.
        assertTrue(mDelegate.claimForceNewInstancePolicy(false));
        // Verify exit type is preserved until onTabbedActivityInitialized is called.
        assertEquals(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ON_STARTUP_WINDOW_POLICY)
    public void
            testClaimForceNewInstancePolicy_lastWindowClosedByApp_featureDisabled_returnsFalse() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeLastSessionExitType(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP);

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
    public void
            testClaimForceNewInstancePolicy_lastWindowClosedByApp_startupPrefIsLast_returnsTrue() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeLastSessionExitType(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.LAST);

        // Act & Verify.
        assertTrue(mDelegate.claimForceNewInstancePolicy(false));
        assertEquals(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void
            testClaimForceNewInstancePolicy_lastWindowClosedByApp_startupPrefIsUrls_returnsFalse() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeLastSessionExitType(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(SessionStartupPref.URLS);

        // Act & Verify.
        assertFalse(mDelegate.claimForceNewInstancePolicy(false));
        assertEquals(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void
            testClaimForceNewInstancePolicy_lastWindowClosedByApp_startupPrefIsNewTab_returnsTrue() {
        // Setup.
        ChromeMultiInstancePersistentStore.writeLastSessionExitType(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(
                SessionStartupPref.NEW_TAB);

        // Act & Verify.
        assertTrue(mDelegate.claimForceNewInstancePolicy(false));
        assertEquals(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
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

    private void setupRecoverableInstances(@LastSessionExitType int exitType) {
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 0, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 0);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        ChromeMultiInstancePersistentStore.writeLastSessionExitType(exitType);

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
