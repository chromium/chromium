// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.XPLAT_SYNCED_SETUP;
import static org.chromium.chrome.browser.ntp_customization.ntp_cards.NtpCardsMediator.MODULE_TYPE_TO_USER_PREFS_KEY;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureListJni;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.prefs.LocalStatePrefsJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.prefs.CrossDevicePrefTrackerFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.ToolbarPositionAndSource;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.CrossDevicePrefTracker;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.CrossDevicePrefTracker.CrossDevicePrefTrackerObserver;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.ServiceStatus;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Supplier;

/** Unit tests for {@link CrossDeviceSettingImporter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(XPLAT_SYNCED_SETUP)
@DisableFeatures(CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)
public class CrossDeviceSettingImporterUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock private Supplier<SnackbarManager> mSnackbarManagerSupplier;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Snackbar mSnackbar;
    @Mock private Tab mTab;
    @Mock private Tab mTab2;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;
    @Mock private LocalStatePrefs.Natives mLocalStatePrefsNatives;
    @Mock private PrefService mLocalPrefService;
    @Mock private CrossDevicePrefTracker mCrossDevicePrefTracker;
    @Mock private SyncedSetUpUtilsBridge.Natives mSyncedSetUpUtilsBridgeNatives;
    @Mock private FeatureList.Natives mFeatureListNatives;
    @Mock private LibraryLoader mLibraryLoader;

    @Captor private ArgumentCaptor<ModalDialogManagerObserver> mModalDialogManagerObserverCaptor;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;
    @Captor private ArgumentCaptor<CrossDevicePrefTrackerObserver> mTrackerObserverCaptor;

    private final SettableNullableObservableSupplier<Tab> mActivityTabSupplier =
            ObservableSuppliers.createNullable();
    private Activity mActivity;
    private CrossDeviceSettingImporter mCrossDeviceSettingImporter;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mActivityTabSupplier.set(mTab);

        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        // UI and Activity mocks.
        when(mModalDialogManagerSupplier.get()).thenReturn(mModalDialogManager);
        when(mSnackbarManagerSupplier.get()).thenReturn(mSnackbarManager);

        // Tab and Profile mocks.
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);

        // SharedPreferences setup.
        SharedPreferencesManager sharedPrefManager = ChromeSharedPreferences.getInstance();
        sharedPrefManager.disableKeyCheckerForTesting();
        sharedPrefManager.writeBoolean(
                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);
        sharedPrefManager.writeBoolean(
                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, false);

        // PrefService and ConfigManager mocks.
        UserPrefs.setPrefServiceForTesting(mPrefService);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);

        // Native pref mocks.
        LocalStatePrefs.setNativePrefsLoadedForTesting(true);
        LocalStatePrefsJni.setInstanceForTesting(mLocalStatePrefsNatives);
        when(mLocalStatePrefsNatives.getPrefService()).thenReturn(mLocalPrefService);

        // Sync and Cross-Device tracker mocks.
        CrossDevicePrefTrackerFactory.setInstanceForTesting(mCrossDevicePrefTracker);
        SyncedSetUpUtilsBridgeJni.setInstanceForTesting(mSyncedSetUpUtilsBridgeNatives);

        // Library and Feature flags mocks.
        FeatureListJni.setInstanceForTesting(mFeatureListNatives);
        when(mFeatureListNatives.isInitialized()).thenReturn(true);
        LibraryLoader.setLibraryLoaderForTesting(mLibraryLoader);
        when(mLibraryLoader.isInitialized()).thenReturn(true);

        mUserActionTester = new UserActionTester();
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private CrossDeviceSettingImporter initializeCrossDeviceSettingImporter() {
        mCrossDeviceSettingImporter =
                new CrossDeviceSettingImporter(
                        mActivityLifecycleDispatcher,
                        mActivityTabSupplier,
                        mActivity,
                        mModalDialogManagerSupplier,
                        mSnackbarManagerSupplier);
        return mCrossDeviceSettingImporter;
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX);
    }

    @Test
    public void testShowSnackbarAfterDialogs_noDialogs() {
        when(mModalDialogManager.isShowing()).thenReturn(false);
        initializeCrossDeviceSettingImporter().showSnackbarAfterDialogs(mSnackbar, false);
        verify(mSnackbarManager).showSnackbar(mSnackbar);
    }

    @Test
    public void testShowSnackbarAfterDialogs_withDialog() {
        when(mModalDialogManager.isShowing()).thenReturn(true);
        initializeCrossDeviceSettingImporter().showSnackbarAfterDialogs(mSnackbar, false);
        verify(mModalDialogManager).addObserver(mModalDialogManagerObserverCaptor.capture());

        // Simulate dialog dismissal.
        mModalDialogManagerObserverCaptor.getValue().onLastDialogDismissed();
        verify(mSnackbarManager).showSnackbar(mSnackbar);
    }

    @Test
    public void testAskToApplyNtpSettingImportIfNeeded_differs() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false);
        when(mPrefService.isDefaultValuePreference(Pref.MAGIC_STACK_HOME_MODULE_ENABLED))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter()
                .askToApplyNtpSettingImportIfNeeded(
                        preferencesToApply, /* onlyOmniboxPosition= */ false);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        assertEquals(
                "The snackbar text should match the ask-to-apply message.",
                mActivity.getString(R.string.synced_set_up_snackbar_ask_to_apply),
                snackbar.getTextForTesting());
        assertEquals(
                "The snackbar action text should be 'apply'.",
                mActivity.getString(R.string.apply),
                snackbar.getActionText());

        // Simulate clicking the action button.
        snackbar.getController().onAction(null);

        // Verify that the preference is changed and the "Undo" snackbar is shown.
        verify(mHomeModulesConfigManager).setPrefAllCardsEnabled(false);
        verify(mSnackbarManager, times(2)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar undoSnackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "The confirmation snackbar text should match the applied confirmation message.",
                mActivity.getString(R.string.synced_set_up_snackbar_applied_confirmation),
                undoSnackbar.getTextForTesting());
        assertEquals(
                "The confirmation snackbar action text should be 'undo'.",
                mActivity.getString(R.string.undo),
                undoSnackbar.getActionText());
    }

    @Test
    public void testAskToApplyNtpSettingImportIfNeeded_noDiffs() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, true);
        when(mPrefService.isDefaultValuePreference(Pref.MAGIC_STACK_HOME_MODULE_ENABLED))
                .thenReturn(true);
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);
        for (Integer moduleType : MODULE_TYPE_TO_USER_PREFS_KEY.keySet()) {
            @Nullable String key = MODULE_TYPE_TO_USER_PREFS_KEY.get(moduleType);
            if (key == null) continue;

            preferencesToApply.put(key, true);
            when(mPrefService.isDefaultValuePreference(key)).thenReturn(true);
            when(mPrefService.getBoolean(key)).thenReturn(true);
        }

        initializeCrossDeviceSettingImporter()
                .askToApplyNtpSettingImportIfNeeded(
                        preferencesToApply, /* onlyOmniboxPosition= */ false);

        verify(mSnackbarManager, times(0)).showSnackbar(mSnackbarCaptor.capture());
    }

    @Test
    public void testUndo() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false);
        when(mPrefService.isDefaultValuePreference(Pref.MAGIC_STACK_HOME_MODULE_ENABLED))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter()
                .askToApplyNtpSettingImportIfNeeded(
                        preferencesToApply, /* onlyOmniboxPosition= */ false);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        snackbar.getController().onAction(null);

        // Verify that the "Undo" snackbar is shown.
        verify(mHomeModulesConfigManager).setPrefAllCardsEnabled(false);
        verify(mSnackbarManager, times(2)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar undoSnackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "The confirmation snackbar text should match the applied confirmation message.",
                mActivity.getString(R.string.synced_set_up_snackbar_applied_confirmation),
                undoSnackbar.getTextForTesting());
        assertEquals(
                "The confirmation snackbar action text should be 'undo'.",
                mActivity.getString(R.string.undo),
                undoSnackbar.getActionText());

        // Simulate clicking the "Undo" action button.
        undoSnackbar.getController().onAction(null);

        // Verify that the preference is changed back and the "Redo" snackbar is shown.
        verify(mHomeModulesConfigManager).setPrefAllCardsEnabled(true);
        verify(mSnackbarManager, times(3)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar redoSnackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "The undo confirmation snackbar text should match the removed confirmation"
                        + " message.",
                mActivity.getString(R.string.synced_set_up_snackbar_removed_confirmation),
                redoSnackbar.getTextForTesting());
        assertEquals(
                "The undo confirmation snackbar action text should be 'redo'.",
                mActivity.getString(R.string.redo),
                redoSnackbar.getActionText());
    }

    @Test
    public void testRedo() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false);
        when(mPrefService.isDefaultValuePreference(Pref.MAGIC_STACK_HOME_MODULE_ENABLED))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter()
                .askToApplyNtpSettingImportIfNeeded(
                        preferencesToApply, /* onlyOmniboxPosition= */ false);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        snackbar.getController().onAction(null);
        verify(mSnackbarManager, times(2)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar undoSnackbar = mSnackbarCaptor.getValue();

        // Simulate clicking the "Undo" action button.
        undoSnackbar.getController().onAction(null);
        verify(mSnackbarManager, times(3)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar redoSnackbar = mSnackbarCaptor.getValue();

        // Simulate clicking the "Redo" action button.
        redoSnackbar.getController().onAction(null);

        // Verify that the preference is changed back and the "Undo" snackbar is shown again.
        verify(mHomeModulesConfigManager, times(2)).setPrefAllCardsEnabled(false);
        verify(mSnackbarManager, times(4)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar secondUndoSnackbar = mSnackbarCaptor.getValue();
        assertEquals(
                "The redo confirmation snackbar text should match the applied confirmation"
                        + " message.",
                mActivity.getString(R.string.synced_set_up_snackbar_applied_confirmation),
                secondUndoSnackbar.getTextForTesting());
        assertEquals(
                "The redo confirmation snackbar action text should be 'undo'.",
                mActivity.getString(R.string.undo),
                secondUndoSnackbar.getActionText());
    }

    @Test
    public void testAskToApplyNtpSettingImportIfNeeded_OmniboxOnly_differs() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.DEVICE_INFO_TRACKER_MISSING);

        initializeCrossDeviceSettingImporter()
                .askToApplyNtpSettingImportIfNeeded(
                        preferencesToApply, /* onlyOmniboxPosition= */ true);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        // Simulate clicking the action button.
        snackbar.getController().onAction(null);

        // Verify that only the local state preference is changed.
        verify(mLocalPrefService, atLeastOnce())
                .setBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        verify(mHomeModulesConfigManager, never()).setPrefAllCardsEnabled(any(Boolean.class));
        assertTrue(
                "The 'Apply' user action for Omnibox position should be recorded.",
                mUserActionTester
                        .getActions()
                        .contains("Android.CrossDeviceSettingImport.OmniboxPosition.Apply"));
    }

    @Test
    public void testAskToApplyNtpSettingImportIfNeeded_OmniboxOnly_noDiffs() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, true);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        initializeCrossDeviceSettingImporter()
                .askToApplyNtpSettingImportIfNeeded(
                        preferencesToApply, /* onlyOmniboxPosition= */ true);

        verify(mSnackbarManager, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testImportedSettingsHavePreferenceChange_includesOmnibox() {
        // Test that when onlyOmniboxPosition=false, omnibox changes still trigger the snackbar.
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        // Other preferences match current.
        when(mPrefService.isDefaultValuePreference(any(String.class))).thenReturn(true);
        when(mPrefService.getBoolean(any(String.class))).thenReturn(true);

        initializeCrossDeviceSettingImporter()
                .askToApplyNtpSettingImportIfNeeded(
                        preferencesToApply, /* onlyOmniboxPosition= */ false);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testRecordUma_UndoRedo() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        initializeCrossDeviceSettingImporter()
                .askToApplyNtpSettingImportIfNeeded(
                        preferencesToApply, /* onlyOmniboxPosition= */ true);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        snackbar.getController().onAction(null); // Apply

        verify(mSnackbarManager, times(2)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar undoSnackbar = mSnackbarCaptor.getValue();
        undoSnackbar.getController().onAction(null); // Undo

        assertTrue(
                "The 'Undo' user action for Omnibox position should be recorded.",
                mUserActionTester
                        .getActions()
                        .contains("Android.CrossDeviceSettingImport.OmniboxPosition.Undo"));

        verify(mSnackbarManager, times(3)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar redoSnackbar = mSnackbarCaptor.getValue();
        redoSnackbar.getController().onAction(null); // Redo

        assertTrue(
                "The 'Redo' user action for Omnibox position should be recorded.",
                mUserActionTester
                        .getActions()
                        .contains("Android.CrossDeviceSettingImport.OmniboxPosition.Redo"));
    }

    @Test
    public void testOnTabChange_TrackerReady_SettingsImported() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, false);

        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(0L);

        // Use remote preferences that differ from local.
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        // Simulate tab change to NTP (settings import).
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker, never()).addObserver(any());
        // Verify snackbar is shown (availableImmediately = true means applyAndNotifySettingImport).
        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                "The preference for having imported all settings should be set to true.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    public void testOnTabChange_TrackerUnavailable_SetsImportedTrue() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, false);

        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.SYNC_NOT_CONFIGURED);
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(0L);

        // Simulate tab change to NTP.
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker, never()).addObserver(any());
        // Should STILL set the shared preference to true, to mark it as "tried".
        assertTrue(
                "The preference for having imported all settings should be set to true even if "
                        + "the tracker is unavailable.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    @DisableFeatures(XPLAT_SYNCED_SETUP)
    public void testOnTabChange_FeatureDisabled_NoAction() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker, never()).getServiceStatus();
        assertTrue(
                "The preference for having imported all settings should not be set if the "
                        + "feature is disabled.",
                !ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    public void testOnTabChange_ProfileNull_NoAction() {
        when(mTab.getProfile()).thenReturn(null);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker, never()).getServiceStatus();
    }

    @Test
    public void testOnTabChange_TrackerNull_NoAction() {
        CrossDevicePrefTrackerFactory.setInstanceForTesting(null);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker, never()).getServiceStatus();
    }

    @Test
    public void testOnTabChange_TrackerNotReady_LocalDeviceInfoMissing_Waits() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, false);

        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.LOCAL_DEVICE_INFO_MISSING);
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(0L);

        // Use remote preferences that differ from local.
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        // Simulate tab change.
        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker).addObserver(mTrackerObserverCaptor.capture());
        // Haven't imported yet.
        assertTrue(
                "The preference for having imported all settings should not be set yet.",
                !ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));

        // Simulate tracker becoming ready.
        mTrackerObserverCaptor.getValue().onServiceStatusChanged(ServiceStatus.AVAILABLE);

        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                "The preference for having imported all settings should be set once the "
                        + "tracker becomes ready.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    public void testOnTabChange_AlreadyImported_NoSnackbar() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, true);

        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testGetPrefsFromRemoteDevice_StripsPrefix() {
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(
                        "cross_device.home.module.magic_stack.enabled",
                        true,
                        "home.module.tips.enabled",
                        false));
        Map<String, Object> result =
                initializeCrossDeviceSettingImporter()
                        .getPrefsFromRemoteDevice(mCrossDevicePrefTracker, mProfile);

        assertEquals("The result map should contain two preferences.", 2, result.size());
        assertTrue(
                "The 'magic_stack.enabled' preference should be true.",
                (Boolean) result.get("home.module.magic_stack.enabled"));
        assertTrue(
                "The 'tips.enabled' preference should be false.",
                !(Boolean) result.get("home.module.tips.enabled"));
    }

    @Test
    public void testOnTabChange_TrackerReady_OmniboxImported() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, false);

        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(0L);

        // Use remote preferences that differ from local.
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false));
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        // Simulate tab change to a non-NTP (omnibox import only).
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker, never()).addObserver(any());
        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                "The preference for having imported bottom omnibox should be set to true.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, false));
    }

    @Test
    public void testOnTabChange_TrackerNotReady_WaitsAndThenImports() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, false);

        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.DEVICE_INFO_TRACKER_MISSING);
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(0L);

        // Use remote preferences that differ from local.
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        // Simulate tab change.
        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker).addObserver(mTrackerObserverCaptor.capture());
        // Haven't imported yet.
        assertTrue(
                "The preference for having imported all settings should not be set yet.",
                !ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));

        // Simulate tracker becoming ready.
        mTrackerObserverCaptor.getValue().onServiceStatusChanged(ServiceStatus.AVAILABLE);

        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                "The preference for having imported all settings should be set once the "
                        + "tracker becomes ready.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    public void testAskToApplyNtpSettingImportIfNeeded_honorsImportedSettings() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, true);

        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(preferencesToApply);

        // Even if there are diffs, it should return early if onlyOmniboxPosition=false because
        // CROSS_DEVICE_IMPORTED_ALL_SETTINGS is true.
        initializeCrossDeviceSettingImporter()
                .onCrossDevicePrefTrackerReady(
                        mCrossDevicePrefTracker, ServiceStatus.AVAILABLE, mProfile, mTab, true);

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testAskToApplyNtpSettingImportIfNeeded_honorsImportedOmnibox() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, true);

        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        initializeCrossDeviceSettingImporter()
                .onCrossDevicePrefTrackerReady(
                        mCrossDevicePrefTracker, ServiceStatus.AVAILABLE, mProfile, mTab, true);

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testApplyLocalStateSettings_UpdatesAddressBarPreference() {
        when(mLocalPrefService.hasPrefPath(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, true);
        // Local state is currently TOP (false).
        AtomicBoolean isOmniboxInBottomPosition = new AtomicBoolean(false);
        doAnswer(
                        (inv) -> {
                            isOmniboxInBottomPosition.set(inv.getArgument(1));
                            return null;
                        })
                .when(mLocalPrefService)
                .setBoolean(any(String.class), anyBoolean());
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION))
                .thenAnswer((inv) -> isOmniboxInBottomPosition.get());
        // ChromeSharedPref is currently TOP_SETTINGS.
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED,
                        ToolbarPositionAndSource.TOP_SETTINGS);

        initializeCrossDeviceSettingImporter()
                .askToApplyNtpSettingImportIfNeeded(
                        preferencesToApply, /* onlyOmniboxPosition= */ true);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        // Simulate clicking the action button.
        snackbar.getController().onAction(null);

        // Verify that the local state preference is changed.
        verify(mLocalPrefService, atLeastOnce())
                .setBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, true);

        // Verify that AddressBarPreference was updated.
        assertEquals(
                "Expected toolbar position to be set to BOTTOM_SETTINGS",
                ToolbarPositionAndSource.BOTTOM_SETTINGS,
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED));
    }

    @Test
    public void testTabObserverManagement() {
        initializeCrossDeviceSettingImporter();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mTab).addObserver(any(TabObserver.class));

        // Simulate tab change.
        mActivityTabSupplier.set(mTab2);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mTab).removeObserver(any(TabObserver.class));
        verify(mTab2).addObserver(any(TabObserver.class));

        // Simulate destroy.
        mCrossDeviceSettingImporter.destroy();
        verify(mTab2).removeObserver(any(TabObserver.class));
        assertTrue(!mActivityTabSupplier.hasObservers());
    }
}
