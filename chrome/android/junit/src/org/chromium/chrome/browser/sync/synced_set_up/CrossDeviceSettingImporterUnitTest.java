// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS;
import static org.chromium.chrome.browser.ntp_customization.ntp_cards.NtpCardsMediator.MODULE_TYPE_TO_USER_PREFS_KEY;
import static org.chromium.chrome.browser.ntp_customization.theme_sync.ServiceStatus.ACTIVE;
import static org.chromium.chrome.browser.ntp_customization.theme_sync.ServiceStatus.INITIALIZING;
import static org.chromium.chrome.browser.ntp_customization.theme_sync.ServiceStatus.SYNC_DISABLED;

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

import org.chromium.base.FeatureOverrides;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.CrossDeviceThemeTracker;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.prefs.LocalStatePrefsJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.prefs.CrossDevicePrefTrackerFactory;
import org.chromium.chrome.browser.sync.synced_set_up.CrossDeviceSettingImporter.CrossDeviceSettingImportOutcome;
import org.chromium.chrome.browser.sync.synced_set_up.CrossDeviceSettingImporter.SyncedSetupSettings;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.ToolbarPositionAndSource;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.CrossDevicePrefTracker;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.CrossDevicePrefTracker.CrossDevicePrefTrackerObserver;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.ServiceStatus;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Supplier;

/** Unit tests for {@link CrossDeviceSettingImporter}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)
@EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
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
    @Mock private CrossDeviceThemeTracker.Natives mCrossDeviceThemeTrackerNatives;
    @Mock private CrossDeviceThemeTracker mCrossDeviceThemeTracker;
    @Mock private SyncedSetUpUtilsBridge.Natives mSyncedSetUpUtilsBridgeNatives;
    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    @Mock private SyncService mSyncService;

    @Captor private ArgumentCaptor<ModalDialogManagerObserver> mModalDialogManagerObserverCaptor;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;
    @Captor private ArgumentCaptor<CrossDevicePrefTrackerObserver> mPrefTrackerObserverCaptor;
    @Captor private ArgumentCaptor<CrossDeviceThemeTracker.Observer> mThemeTrackerObserverCaptor;

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
                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_NON_NTP_SETTINGS, false);
        sharedPrefManager.writeBoolean(
                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, false);

        // PrefService and ConfigManager mocks.
        UserPrefs.setPrefServiceForTesting(mPrefService);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);
        NtpCustomizationConfigManager.setInstanceForTesting(mNtpCustomizationConfigManager);
        SyncServiceFactory.setInstanceForTesting(mSyncService);

        // Native pref mocks.
        LocalStatePrefs.setNativePrefsLoadedForTesting(true);
        LocalStatePrefsJni.setInstanceForTesting(mLocalStatePrefsNatives);
        when(mLocalStatePrefsNatives.getPrefService()).thenReturn(mLocalPrefService);

        // Sync and Cross-Device tracker mocks.
        CrossDevicePrefTrackerFactory.setInstanceForTesting(mCrossDevicePrefTracker);
        CrossDeviceThemeTracker.setInstanceForTesting(mCrossDeviceThemeTrackerNatives);
        when(mCrossDeviceThemeTrackerNatives.getForProfile(mProfile))
                .thenReturn(mCrossDeviceThemeTracker);
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(ACTIVE);
        SyncedSetUpUtilsBridgeJni.setInstanceForTesting(mSyncedSetUpUtilsBridgeNatives);

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
                .removeKey(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_NON_NTP_SETTINGS);
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
                .askToApplySettingImportIfNeeded(mProfile, preferencesToApply, /* nonNtp= */ false);

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
                .askToApplySettingImportIfNeeded(mProfile, preferencesToApply, /* nonNtp= */ false);

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
                .askToApplySettingImportIfNeeded(mProfile, preferencesToApply, /* nonNtp= */ false);

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
                .askToApplySettingImportIfNeeded(mProfile, preferencesToApply, /* nonNtp= */ false);

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
    public void testAskToApplySettingImportIfNeeded_NonNtp_differs() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.DEVICE_INFO_TRACKER_MISSING);

        initializeCrossDeviceSettingImporter()
                .askToApplySettingImportIfNeeded(mProfile, preferencesToApply, /* nonNtp= */ true);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        // Simulate clicking the action button.
        snackbar.getController().onAction(null);

        // Verify that only the local state preference is changed.
        verify(mLocalPrefService, atLeastOnce())
                .setBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        verify(mHomeModulesConfigManager, never()).setPrefAllCardsEnabled(any(Boolean.class));
        assertTrue(
                "The 'Apply' user action for non-NTP settings should be recorded.",
                mUserActionTester
                        .getActions()
                        .contains("Android.CrossDeviceSettingImport.NonNtp.Apply"));
    }

    @Test
    public void testAskToApplySettingImportIfNeeded_NonNtp_noDiffs() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, true);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        initializeCrossDeviceSettingImporter()
                .askToApplySettingImportIfNeeded(mProfile, preferencesToApply, /* nonNtp= */ true);

        verify(mSnackbarManager, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testImportedSettingsHavePreferenceChange_includesOmnibox() {
        // Test that when nonNtp=false, omnibox changes still trigger the snackbar.
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        // Other preferences match current.
        when(mPrefService.isDefaultValuePreference(any(String.class))).thenReturn(true);
        when(mPrefService.getBoolean(any(String.class))).thenReturn(true);

        initializeCrossDeviceSettingImporter()
                .askToApplySettingImportIfNeeded(mProfile, preferencesToApply, /* nonNtp= */ false);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testRecordAction_UndoRedo() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        initializeCrossDeviceSettingImporter()
                .askToApplySettingImportIfNeeded(mProfile, preferencesToApply, /* nonNtp= */ true);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        snackbar.getController().onAction(null); // Apply

        verify(mSnackbarManager, times(2)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar undoSnackbar = mSnackbarCaptor.getValue();
        undoSnackbar.getController().onAction(null); // Undo

        assertTrue(
                "The 'Undo' user action for non-NTP settings should be recorded.",
                mUserActionTester
                        .getActions()
                        .contains("Android.CrossDeviceSettingImport.NonNtp.Undo"));

        verify(mSnackbarManager, times(3)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar redoSnackbar = mSnackbarCaptor.getValue();
        redoSnackbar.getController().onAction(null); // Redo

        assertTrue(
                "The 'Redo' user action for non-NTP settings should be recorded.",
                mUserActionTester
                        .getActions()
                        .contains("Android.CrossDeviceSettingImport.NonNtp.Redo"));
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
        doTestOnTabChange_TrackerNotReady_Waits(ServiceStatus.LOCAL_DEVICE_INFO_MISSING);
    }

    @Test
    public void testOnTabChange_TrackerNotReady_WaitingForInitialSync_Waits() {
        doTestOnTabChange_TrackerNotReady_Waits(ServiceStatus.WAITING_FOR_INITIAL_SYNC);
    }

    @Test
    public void testOnTabChange_TrackerNotReady_SyncNotConfiguredAndLocalDeviceInfoMissing_Waits() {
        doTestOnTabChange_TrackerNotReady_Waits(
                ServiceStatus.SYNC_NOT_CONFIGURED_AND_LOCAL_DEVICE_INFO_MISSING);
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
                        .getPrefsFromRemoteDevice(mProfile, mCrossDevicePrefTracker);

        assertEquals("The result map should contain two preferences.", 2, result.size());
        assertTrue(
                "The 'magic_stack.enabled' preference should be true.",
                (Boolean) result.get("home.module.magic_stack.enabled"));
        assertTrue(
                "The 'tips.enabled' preference should be false.",
                !(Boolean) result.get("home.module.tips.enabled"));
    }

    @Test
    public void testOnTabChange_TrackerReady_NonNtpSettingsImported() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_NON_NTP_SETTINGS, false);

        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(0L);

        // Use remote preferences that differ from local.
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false));
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        // Simulate tab change to a non-NTP (non-NTP settings import only).
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker, never()).addObserver(any());
        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                "The preference for having imported non-NTP settings should be set to true.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_NON_NTP_SETTINGS,
                                false));
    }

    @Test
    public void testOnTabChange_TrackerNotReady_WaitsAndThenImports() {
        doTestOnTabChange_TrackerNotReady_Waits(ServiceStatus.DEVICE_INFO_TRACKER_MISSING);
    }

    @Test
    public void testAskToApplySettingImportIfNeeded_honorsImportedSettings() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, true);

        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(preferencesToApply);

        // Even if there are diffs, it should return early if nonNtp=false because
        // CROSS_DEVICE_IMPORTED_ALL_SETTINGS is true.
        initializeCrossDeviceSettingImporter()
                .onDependenciesReady(
                        mCrossDevicePrefTracker, ServiceStatus.AVAILABLE, mProfile, mTab, true);

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testAskToApplySettingImportIfNeeded_honorsImportedNonNtpSettings() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_NON_NTP_SETTINGS, true);

        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        initializeCrossDeviceSettingImporter()
                .onDependenciesReady(
                        mCrossDevicePrefTracker, ServiceStatus.AVAILABLE, mProfile, mTab, true);

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testMigration_FromBottomOmniboxToNonNtpSettings() {
        // Old key was set, new key was not yet set.
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_NON_NTP_SETTINGS);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, true);

        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        initializeCrossDeviceSettingImporter()
                .onDependenciesReady(
                        mCrossDevicePrefTracker, ServiceStatus.AVAILABLE, mProfile, mTab, true);

        // Should return early and migrate the key to true.
        verify(mSnackbarManager, never()).showSnackbar(any());
        assertTrue(
                "Old key value should migrate into CROSS_DEVICE_IMPORTED_NON_NTP_SETTINGS.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_NON_NTP_SETTINGS,
                                false));
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
                .askToApplySettingImportIfNeeded(mProfile, preferencesToApply, /* nonNtp= */ true);

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
    public void testOnTabChange_LocalStateNotReady_WaitsAndThenImports() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);

        LocalStatePrefs.setNativePrefsLoadedForTesting(false);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);

        // Use remote preferences that differ from local.
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        // Haven't imported yet.
        verify(mSnackbarManager, never()).showSnackbar(any());
        assertTrue(
                "The preference for having imported all settings should not be set yet.",
                !ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));

        // Simulate LocalState becoming ready.
        LocalStatePrefs.setNativePrefsLoadedForTesting(true);

        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                "The preference for having imported all settings should be set once LocalState"
                        + " becomes ready.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    public void testOnTabChange_TrackerAndLocalStateNotReady_WaitsAndThenImports() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);

        LocalStatePrefs.setNativePrefsLoadedForTesting(false);
        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.WAITING_FOR_INITIAL_SYNC);

        // Use remote preferences that differ from local.
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker).addObserver(mPrefTrackerObserverCaptor.capture());

        // Simulate tracker becoming ready.
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        mPrefTrackerObserverCaptor.getValue().onServiceStatusChanged(ServiceStatus.AVAILABLE);

        // Still haven't imported yet because LocalState is not ready.
        verify(mSnackbarManager, never()).showSnackbar(any());

        // Simulate LocalState becoming ready.
        LocalStatePrefs.setNativePrefsLoadedForTesting(true);

        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                "The preference for having imported all settings should be set once both tracker"
                        + " and LocalState become ready.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    public void testDestroy_RemovesLocalStateObserver() {
        LocalStatePrefs.setNativePrefsLoadedForTesting(false);
        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        AtomicBoolean called = new AtomicBoolean(false);
        LocalStatePrefs.addObserver(() -> called.set(true));

        mCrossDeviceSettingImporter.destroy();

        // Simulate LocalState becoming ready.
        LocalStatePrefs.setNativePrefsLoadedForTesting(true);

        // The importer's observer should have been removed, so it shouldn't trigger an import.
        // We check this by verifying that mSnackbarManager.showSnackbar was never called.
        verify(mSnackbarManager, never()).showSnackbar(any());
        assertTrue("Our own test observer should still be called.", called.get());
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

    @Test
    public void testOnServiceStatusChanged_TabBecomesNull_NoCrash() {
        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.DEVICE_INFO_TRACKER_MISSING);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker).addObserver(mPrefTrackerObserverCaptor.capture());

        // Simulate tab becoming null.
        mActivityTabSupplier.set(null);

        // Simulate tracker becoming ready.
        // This should NOT crash even though mActivityTabSupplier.get() is null.
        mPrefTrackerObserverCaptor.getValue().onServiceStatusChanged(ServiceStatus.AVAILABLE);
    }

    @Test
    public void testOnServiceStatusChanged_ProfileBecomesNull_NoCrash() {
        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.DEVICE_INFO_TRACKER_MISSING);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker).addObserver(mPrefTrackerObserverCaptor.capture());

        // Simulate profile becoming null on the tab.
        when(mTab.getProfile()).thenReturn(null);

        // Simulate tracker becoming ready.
        // This should NOT crash even though tab.getProfile() is null.
        mPrefTrackerObserverCaptor.getValue().onServiceStatusChanged(ServiceStatus.AVAILABLE);
    }

    @Test
    public void testOnTabChange_TrackerNotReady_ObserverRemovedWhenReady() {
        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.LOCAL_DEVICE_INFO_MISSING);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker).addObserver(mPrefTrackerObserverCaptor.capture());

        // Simulate tracker becoming ready.
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        mPrefTrackerObserverCaptor.getValue().onServiceStatusChanged(ServiceStatus.AVAILABLE);

        verify(mCrossDevicePrefTracker).removeObserver(mPrefTrackerObserverCaptor.getValue());
    }

    @Test
    public void testDestroy_RemovesTrackerObserver() {
        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.LOCAL_DEVICE_INFO_MISSING);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker).addObserver(mPrefTrackerObserverCaptor.capture());

        mCrossDeviceSettingImporter.destroy();

        verify(mCrossDevicePrefTracker).removeObserver(mPrefTrackerObserverCaptor.getValue());
    }

    private void doTestOnTabChange_TrackerNotReady_Waits(int status) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, false);

        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(status);
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(0L);

        // Use remote preferences that differ from local.
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        // Simulate tab change.
        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker).addObserver(mPrefTrackerObserverCaptor.capture());
        // Haven't imported yet.
        assertTrue(
                "The preference for having imported all settings should not be set yet.",
                !ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));

        // Simulate tracker becoming ready.
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        mPrefTrackerObserverCaptor.getValue().onServiceStatusChanged(ServiceStatus.AVAILABLE);

        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                "The preference for having imported all settings should be set once the "
                        + "tracker becomes ready.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnTabChange_ThemesEnabled_TrackerReady_SettingsImported() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, false);

        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(0L);
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(ACTIVE);

        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDeviceThemeTracker, never()).addObserver(any());
        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                "The preference for having imported all settings should be set to true.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnTabChange_ThemesEnabled_ThemeTrackerInitializing_WaitsAndImports() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, false);

        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(0L);
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(INITIALIZING);

        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDeviceThemeTracker).addObserver(mThemeTrackerObserverCaptor.capture());
        verify(mSnackbarManager, never()).showSnackbar(any());
        assertTrue(
                "Settings should not be marked imported yet while theme tracker is initializing.",
                !ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));

        // Simulate theme tracker becoming active.
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(ACTIVE);
        mThemeTrackerObserverCaptor.getValue().onStatusChanged(ACTIVE);

        verify(mCrossDeviceThemeTracker).removeObserver(mThemeTrackerObserverCaptor.getValue());
        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                "Settings should be marked imported once theme tracker becomes active.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnTabChange_ThemesEnabled_ThemeTrackerSyncDisabled_WaitsAndImports() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, false);

        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(0L);
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(INITIALIZING);

        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDeviceThemeTracker).addObserver(mThemeTrackerObserverCaptor.capture());

        // Simulate theme tracker transitioning to SYNC_DISABLED.
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(SYNC_DISABLED);
        mThemeTrackerObserverCaptor.getValue().onStatusChanged(SYNC_DISABLED);

        verify(mCrossDeviceThemeTracker).removeObserver(mThemeTrackerObserverCaptor.getValue());
        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                "Settings should be marked imported when theme tracker is SYNC_DISABLED.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnTabChange_ThemesEnabled_AllThreeNotReady_WaitsAndThenImports() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false);

        LocalStatePrefs.setNativePrefsLoadedForTesting(false);
        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.WAITING_FOR_INITIAL_SYNC);
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(INITIALIZING);

        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDevicePrefTracker).addObserver(mPrefTrackerObserverCaptor.capture());
        verify(mCrossDeviceThemeTracker).addObserver(mThemeTrackerObserverCaptor.capture());

        // Step 1: Pref tracker becomes available, but LocalState and ThemeTracker not ready.
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        mPrefTrackerObserverCaptor.getValue().onServiceStatusChanged(ServiceStatus.AVAILABLE);
        verify(mSnackbarManager, never()).showSnackbar(any());

        // Step 2: Theme tracker becomes active, but LocalState not ready.
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(ACTIVE);
        mThemeTrackerObserverCaptor.getValue().onStatusChanged(ACTIVE);
        verify(mSnackbarManager, never()).showSnackbar(any());

        // Step 3: LocalState becomes ready.
        LocalStatePrefs.setNativePrefsLoadedForTesting(true);

        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                "Settings should be marked imported once all three services are ready.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnTabChange_ThemesEnabled_ThemeTrackerNotReady_TabBecomesNull_NoCrash() {
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(INITIALIZING);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDeviceThemeTracker).addObserver(mThemeTrackerObserverCaptor.capture());

        // Tab becomes null before theme tracker is ready.
        mActivityTabSupplier.set(null);

        mThemeTrackerObserverCaptor.getValue().onStatusChanged(ACTIVE);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnTabChange_ThemesEnabled_ThemeTrackerNotReady_ProfileBecomesNull_NoCrash() {
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(INITIALIZING);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDeviceThemeTracker).addObserver(mThemeTrackerObserverCaptor.capture());

        // Profile becomes null before theme tracker is ready.
        when(mTab.getProfile()).thenReturn(null);

        mThemeTrackerObserverCaptor.getValue().onStatusChanged(ACTIVE);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testDestroy_RemovesThemeTrackerObserver() {
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(INITIALIZING);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDeviceThemeTracker).addObserver(mThemeTrackerObserverCaptor.capture());

        mCrossDeviceSettingImporter.destroy();

        verify(mCrossDeviceThemeTracker).removeObserver(mThemeTrackerObserverCaptor.getValue());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnTabChange_ThemesEnabled_ThemeTrackerNull_NoAction() {
        when(mCrossDeviceThemeTrackerNatives.getForProfile(mProfile)).thenReturn(null);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mCrossDeviceThemeTracker, never()).getServiceStatus();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testGetThemeFromRemoteDevice_ThemesDisabled_ReturnsNull() {
        assertNull(
                initializeCrossDeviceSettingImporter()
                        .getThemeFromRemoteDevice(mProfile, mCrossDevicePrefTracker));
        verify(mCrossDeviceThemeTracker, never()).getThemeForDeviceGuid(any(), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testGetThemeFromRemoteDevice_WithBestMatchGuid_QueriesSpecificGuid() {
        SyncedSetUpUtilsBridge.setBestMatchDeviceGuidForTesting("device_guid_123");
        NtpBackgroundDataColor theme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.DESKTOP, NtpThemeColorId.NTP_COLORS_BLUE, false);
        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), eq("device_guid_123")))
                .thenReturn(theme);

        NtpBackgroundDataBase result =
                initializeCrossDeviceSettingImporter()
                        .getThemeFromRemoteDevice(mProfile, mCrossDevicePrefTracker);

        assertEquals(theme, result);
        verify(mCrossDeviceThemeTracker).getThemeForDeviceGuid(any(), eq("device_guid_123"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testGetThemeFromRemoteDevice_NoBestMatchGuid_QueriesNullGuid() {
        SyncedSetUpUtilsBridge.setBestMatchDeviceGuidForTesting("");
        NtpBackgroundDataColor theme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_BLUE, false);
        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), isNull())).thenReturn(theme);

        NtpBackgroundDataBase result =
                initializeCrossDeviceSettingImporter()
                        .getThemeFromRemoteDevice(mProfile, mCrossDevicePrefTracker);

        assertEquals(theme, result);
        verify(mCrossDeviceThemeTracker).getThemeForDeviceGuid(any(), isNull());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testImportedSettingsHavePreferenceChange_ThemesEnabled_SameTheme_NoChange() {
        NtpBackgroundDataColor theme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_BLUE, false);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(theme);

        SyncedSetupSettings settings = new SyncedSetupSettings(new HashMap<>(), theme);

        when(mPrefService.isDefaultValuePreference(any(String.class))).thenReturn(true);
        when(mPrefService.getBoolean(any(String.class))).thenReturn(true);

        initializeCrossDeviceSettingImporter()
                .askToApplySettingImportIfNeeded(mProfile, settings, /* nonNtp= */ false);

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void
            testImportedSettingsHavePreferenceChange_ThemesEnabled_DifferentTheme_ShowsSnackbar() {
        NtpBackgroundDataColor remoteTheme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.DESKTOP, NtpThemeColorId.NTP_COLORS_BLUE, false);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(null);

        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any())).thenReturn(remoteTheme);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        verify(mNtpCustomizationConfigManager).onBackgroundDataChanged(any(), eq(remoteTheme));
        verify(mNtpCustomizationConfigManager)
                .maybeSaveUserSelectedBackgroundTypeToSharedPreference(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testUndoSettingImport_AndroidTheme_WithThemeChange_DisablesThemesSync() {
        NtpBackgroundDataColor remoteAndroidTheme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_BLUE, false);
        NtpBackgroundDataColor localTheme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_GREEN, false);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(localTheme);

        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any()))
                .thenReturn(remoteAndroidTheme);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);

        // Include a preference change so that synced set up triggers for same-platform.
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar appliedSnackbar = mSnackbarCaptor.getValue();

        // Click Undo.
        appliedSnackbar.getController().onAction(null);

        // Previous theme restored.
        verify(mNtpCustomizationConfigManager).onBackgroundDataChanged(any(), eq(localTheme));
        // Themes sync disabled because candidate was Android and theme actually changed.
        verify(mSyncService).setSelectedType(UserSelectableType.THEMES, false);
        assertEquals(1, mUserActionTester.getActionCount("Android.CrossDeviceSettingImport.Undo"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testUndoSettingImport_AndroidTheme_NoThemeChange_DoesNotDisableThemesSync() {
        NtpBackgroundDataColor theme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_BLUE, false);
        // Remote and local themes are identical (no theme change).
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(theme);

        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any())).thenReturn(theme);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);

        // Include a preference change so that synced set up triggers.
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar appliedSnackbar = mSnackbarCaptor.getValue();

        // Click Undo.
        appliedSnackbar.getController().onAction(null);

        // Themes sync is NOT disabled because no theme change occurred.
        verify(mSyncService, never()).setSelectedType(UserSelectableType.THEMES, false);
        assertEquals(1, mUserActionTester.getActionCount("Android.CrossDeviceSettingImport.Undo"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testUndoSettingImport_DesktopTheme_DoesNotDisableThemesSync() {
        NtpBackgroundDataColor remoteDesktopTheme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.DESKTOP, NtpThemeColorId.NTP_COLORS_BLUE, false);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(null);

        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any()))
                .thenReturn(remoteDesktopTheme);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar appliedSnackbar = mSnackbarCaptor.getValue();

        // Click Undo.
        appliedSnackbar.getController().onAction(null);

        // Previous theme restored (null for default).
        verify(mNtpCustomizationConfigManager).onBackgroundDataChanged(any(), isNull());
        // Sync is NOT disabled for Desktop themes.
        verify(mSyncService, never()).setSelectedType(UserSelectableType.THEMES, false);
        assertEquals(1, mUserActionTester.getActionCount("Android.CrossDeviceSettingImport.Undo"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testRedoSettingImport_AndroidTheme_ReenablesThemesSync() {
        NtpBackgroundDataColor remoteAndroidTheme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_BLUE, false);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(null);

        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any()))
                .thenReturn(remoteAndroidTheme);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);

        // Include a preference change so that synced set up triggers for same-platform.
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        // 1. Initial applied snackbar shown.
        verify(mSnackbarManager, times(1)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar undoSnackbar = mSnackbarCaptor.getValue();

        // 2. Click Undo -> shows Redo snackbar.
        undoSnackbar.getController().onAction(null);
        verify(mSyncService).setSelectedType(UserSelectableType.THEMES, false);
        verify(mSnackbarManager, times(2)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar redoSnackbar = mSnackbarCaptor.getValue();

        // 3. Click Redo -> reenables sync and re-applies theme.
        redoSnackbar.getController().onAction(null);
        verify(mSyncService).setSelectedType(UserSelectableType.THEMES, true);
        assertEquals(1, mUserActionTester.getActionCount("Android.CrossDeviceSettingImport.Redo"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void
            testImportedSettingsHavePreferenceChange_ThemesEnabled_SamePlatformThemeOnly_NoPreferenceChanges_NoSnackbar() {
        NtpBackgroundDataColor remoteAndroidTheme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_BLUE, false);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(null);

        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any()))
                .thenReturn(remoteAndroidTheme);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mPrefService.isDefaultValuePreference(any(String.class))).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        // On same platform (Android), theme change alone does NOT trigger synced setup snackbar.
        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnDependenciesReady_ThemeOnly_SyncDisabledForPrefs_ImportsTheme() {
        NtpBackgroundDataColor remoteTheme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.DESKTOP, NtpThemeColorId.NTP_COLORS_BLUE, false);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(null);

        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any())).thenReturn(remoteTheme);
        // Pref tracker is SYNC_NOT_CONFIGURED, but theme tracker is ready with a theme.
        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.SYNC_NOT_CONFIGURED);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mSnackbarManager).showSnackbar(any());
        verify(mNtpCustomizationConfigManager).onBackgroundDataChanged(any(), eq(remoteTheme));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnTabChange_NonNtpTab_CrossPlatformTheme_ImportsTheme() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        NtpBackgroundDataColor remoteTheme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.DESKTOP, NtpThemeColorId.NTP_COLORS_BLUE, false);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(null);

        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any())).thenReturn(remoteTheme);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);

        // Omnibox setting is not changed.
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(false);
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false));

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        // Themes affect non-NTP pages via the omnibox, so cross-platform theme changes show
        // snackbar.
        verify(mSnackbarManager).showSnackbar(any());
        verify(mNtpCustomizationConfigManager).onBackgroundDataChanged(any(), eq(remoteTheme));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnTabChange_NonNtpTab_SamePlatformThemeOnly_NoSnackbar() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        NtpBackgroundDataColor remoteTheme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_BLUE, false);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(null);

        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any())).thenReturn(remoteTheme);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);

        // Omnibox setting is not changed.
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(false);
        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false));

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        // Same-platform theme syncs automatically in background, so no snackbar.
        verify(mSnackbarManager, never()).showSnackbar(any());
        verify(mNtpCustomizationConfigManager, never()).onBackgroundDataChanged(any(), any());
    }

    @Test
    public void testSyncedSetupSettings_EqualsHashCodeToString() {
        NtpBackgroundDataColor theme1 =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_BLUE, false);
        NtpBackgroundDataColor theme2 =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.ANDROID, NtpThemeColorId.NTP_COLORS_BLUE, false);
        NtpBackgroundDataColor theme3 =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.DESKTOP, NtpThemeColorId.NTP_COLORS_GREEN, false);

        SyncedSetupSettings settings1 = new SyncedSetupSettings(Map.of("pref1", true), theme1);
        SyncedSetupSettings settings2 = new SyncedSetupSettings(Map.of("pref1", true), theme2);
        SyncedSetupSettings settings3 = new SyncedSetupSettings(Map.of("pref1", false), theme1);
        SyncedSetupSettings settings4 = new SyncedSetupSettings(Map.of("pref1", true), theme3);
        SyncedSetupSettings settings5 = new SyncedSetupSettings(Map.of("pref1", true));
        SyncedSetupSettings settings6 = new SyncedSetupSettings(Map.of("pref1", true), null);

        assertEquals(settings1, settings2);
        assertEquals(settings1.hashCode(), settings2.hashCode());
        assertEquals(settings5, settings6);
        assertEquals(settings5.hashCode(), settings6.hashCode());

        assertNotEquals(settings1, settings3);
        assertNotEquals(settings1, settings4);
        assertNotEquals(settings1, settings5);
        assertNotEquals(settings1, null);
        assertNotEquals(settings1, new Object());

        assertTrue(settings1.toString().contains("prefs={pref1=true}"));
        assertTrue(settings1.toString().contains("theme="));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void
            testOnTabChange_ThemesEnabled_PrefSyncDisabled_ThemeSyncActive_NoTheme_NoSettingsToImport() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        CrossDeviceSettingImporter.CROSS_DEVICE_SETTING_IMPORT_OUTCOME_HISTOGRAM,
                        CrossDeviceSettingImportOutcome.NO_SETTINGS_TO_IMPORT);

        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.SYNC_NOT_CONFIGURED);
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(ACTIVE);
        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any())).thenReturn(null);
        when(mPrefService.isDefaultValuePreference(any(String.class))).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mSnackbarManager, never()).showSnackbar(any());
        verify(mNtpCustomizationConfigManager, never()).onBackgroundDataChanged(any(), any());
        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnTabChange_ThemesEnabled_BothSyncTogglesDisabled_SyncNotConfigured() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        CrossDeviceSettingImporter.CROSS_DEVICE_SETTING_IMPORT_OUTCOME_HISTOGRAM,
                        CrossDeviceSettingImportOutcome.SYNC_NOT_CONFIGURED);

        when(mCrossDevicePrefTracker.getServiceStatus())
                .thenReturn(ServiceStatus.SYNC_NOT_CONFIGURED);
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(SYNC_DISABLED);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        verify(mSnackbarManager, never()).showSnackbar(any());
        verify(mNtpCustomizationConfigManager, never()).onBackgroundDataChanged(any(), any());
        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testIsThemeFeatureEnabled_RequiresBothFeatures() {
        CrossDeviceSettingImporter importer = initializeCrossDeviceSettingImporter();
        assertTrue(importer.isThemeFeatureEnabled());
        assertTrue(importer.isThemeImportSnackbarEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testIsThemeFeatureEnabled_ThemeSyncDisabled_ReturnsFalse() {
        CrossDeviceSettingImporter importer = initializeCrossDeviceSettingImporter();
        assertTrue(!importer.isThemeFeatureEnabled());
        assertTrue(!importer.isThemeImportSnackbarEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testIsThemeFeatureEnabled_XplatDisabled_ReturnsFalse() {
        CrossDeviceSettingImporter importer = initializeCrossDeviceSettingImporter();
        assertTrue(!importer.isThemeFeatureEnabled());
        assertTrue(!importer.isThemeImportSnackbarEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnTabChange_ThemeSyncDisabled_DoesNotStallNonThemeImports() {
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mCrossDevicePrefTracker.getNativePtr()).thenReturn(0L);
        when(mCrossDeviceThemeTracker.getServiceStatus()).thenReturn(INITIALIZING);

        SyncedSetUpUtilsBridge.setCrossDeviceSettingsForTesting(
                Map.of(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false));
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        // Theme tracker should NOT be observed because theme sync is disabled.
        verify(mCrossDeviceThemeTracker, never()).addObserver(any());
        // Preference import must NOT be stalled.
        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, false));
    }

    @Test
    public void testObservationOnly_ThemeOnlyChange_NtpTab_EmitsMetric_DoesNotShowSnackbar() {
        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
                .param(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES_OBSERVATION_ONLY, true)
                .apply();

        CrossDeviceSettingImporter importer = initializeCrossDeviceSettingImporter();
        assertTrue(importer.isObservationOnly());
        assertTrue(!importer.isThemeImportSnackbarEnabled());

        NtpBackgroundDataColor remoteDesktopTheme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.DESKTOP, NtpThemeColorId.NTP_COLORS_BLUE, false);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(null);

        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any()))
                .thenReturn(remoteDesktopTheme);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mPrefService.isDefaultValuePreference(any(String.class))).thenReturn(true);

        importer.onTabChangeOrGainFocus(mTab);

        verify(mSnackbarManager, never()).showSnackbar(any());
        verify(mNtpCustomizationConfigManager, never()).onBackgroundDataChanged(any(), any());
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Android.CrossDeviceSettingImport.ObservationOnly"));
        assertEquals(
                0,
                mUserActionTester.getActionCount(
                        "Android.CrossDeviceSettingImport.NonNtp.ObservationOnly"));
    }

    @Test
    public void testObservationOnly_ThemeOnlyChange_NonNtpTab_EmitsMetric_DoesNotShowSnackbar() {
        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
                .param(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES_OBSERVATION_ONLY, true)
                .apply();

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        CrossDeviceSettingImporter importer = initializeCrossDeviceSettingImporter();
        assertTrue(importer.isObservationOnly());
        assertTrue(!importer.isThemeImportSnackbarEnabled());

        NtpBackgroundDataColor remoteDesktopTheme =
                new NtpBackgroundDataColor(
                        mActivity, PlatformType.DESKTOP, NtpThemeColorId.NTP_COLORS_BLUE, false);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(null);

        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any()))
                .thenReturn(remoteDesktopTheme);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mPrefService.isDefaultValuePreference(any(String.class))).thenReturn(true);

        importer.onTabChangeOrGainFocus(mTab);

        verify(mSnackbarManager, never()).showSnackbar(any());
        verify(mNtpCustomizationConfigManager, never()).onBackgroundDataChanged(any(), any());
        assertEquals(
                0,
                mUserActionTester.getActionCount(
                        "Android.CrossDeviceSettingImport.ObservationOnly"));
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "Android.CrossDeviceSettingImport.NonNtp.ObservationOnly"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.XPLAT_SYNCED_SETUP_THEMES)
    public void testOnTabChange_ThemeCollectionWithNullBitmap_DoesNotClobberBackground() {
        CustomBackgroundInfo bgInfo =
                new CustomBackgroundInfo(
                        new GURL("https://example.com/theme.png"),
                        "collection_1",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection remoteTheme =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.DESKTOP, bgInfo, /* previewBitmap= */ null);

        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(null);
        when(mCrossDeviceThemeTracker.getThemeForDeviceGuid(any(), any())).thenReturn(remoteTheme);
        when(mCrossDevicePrefTracker.getServiceStatus()).thenReturn(ServiceStatus.AVAILABLE);
        when(mPrefService.isDefaultValuePreference(any(String.class))).thenReturn(true);

        initializeCrossDeviceSettingImporter().onTabChangeOrGainFocus(mTab);

        // Snackbar is shown because a cross-platform theme change was detected.
        verify(mSnackbarManager).showSnackbar(any());
        // Crucially: onBackgroundDataChanged must NOT be called with a null bitmap!
        verify(mNtpCustomizationConfigManager, never()).onBackgroundDataChanged(any(), any());
    }
}
