// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.prefs.LocalStatePrefsJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;

import java.util.HashMap;
import java.util.Map;
import java.util.function.Supplier;

/** Unit tests for {@link CrossDeviceSettingImporter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
    @Mock private NullableObservableSupplier<Tab> mActivityTabSupplier;
    @Mock private Tab mTab;
    @Mock private Tab mTab2;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;
    @Mock private LocalStatePrefs.Natives mLocalStatePrefsNatives;
    @Mock private PrefService mLocalPrefService;

    @Captor private ArgumentCaptor<ModalDialogManagerObserver> mModalDialogManagerObserverCaptor;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;
    @Captor private ArgumentCaptor<Callback<Tab>> mTabChangeCallbackCaptor;

    private Activity mActivity;
    private CrossDeviceSettingImporter mCrossDeviceSettingImporter;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        when(mModalDialogManagerSupplier.get()).thenReturn(mModalDialogManager);
        when(mSnackbarManagerSupplier.get()).thenReturn(mSnackbarManager);
        when(mActivityTabSupplier.get()).thenReturn(mTab);
        when(mTab.getProfile()).thenReturn(mProfile);

        UserPrefs.setPrefServiceForTesting(mPrefService);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);

        LocalStatePrefs.setNativePrefsLoadedForTesting(true);
        LocalStatePrefsJni.setInstanceForTesting(mLocalStatePrefsNatives);
        when(mLocalStatePrefsNatives.getPrefService()).thenReturn(mLocalPrefService);

        mUserActionTester = new UserActionTester();

        mCrossDeviceSettingImporter =
                new CrossDeviceSettingImporter(
                        mActivityLifecycleDispatcher,
                        mActivityTabSupplier,
                        mActivity,
                        mModalDialogManagerSupplier,
                        mSnackbarManagerSupplier);

        verify(mActivityTabSupplier).addObserver(mTabChangeCallbackCaptor.capture());
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
    }

    @Test
    public void testShowSnackbarAfterDialogs_noDialogs() {
        when(mModalDialogManager.isShowing()).thenReturn(false);
        mCrossDeviceSettingImporter.showSnackbarAfterDialogs(mSnackbar);
        verify(mSnackbarManager).showSnackbar(mSnackbar);
    }

    @Test
    public void testShowSnackbarAfterDialogs_withDialog() {
        when(mModalDialogManager.isShowing()).thenReturn(true);
        mCrossDeviceSettingImporter.showSnackbarAfterDialogs(mSnackbar);
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

        mCrossDeviceSettingImporter.askToApplyNtpSettingImportIfNeeded(
                preferencesToApply, /* onlyOmniboxPosition= */ false);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        assertEquals(
                mActivity.getString(R.string.synced_set_up_snackbar_ask_to_apply),
                snackbar.getTextForTesting());
        assertEquals(mActivity.getString(R.string.apply), snackbar.getActionText());

        // Simulate clicking the action button.
        snackbar.getController().onAction(null);

        // Verify that the preference is changed and the "Undo" snackbar is shown.
        verify(mHomeModulesConfigManager).setPrefAllCardsEnabled(false);
        verify(mSnackbarManager, times(2)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar undoSnackbar = mSnackbarCaptor.getValue();
        assertEquals(
                mActivity.getString(R.string.synced_set_up_snackbar_applied_confirmation),
                undoSnackbar.getTextForTesting());
        assertEquals(mActivity.getString(R.string.undo), undoSnackbar.getActionText());
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

        mCrossDeviceSettingImporter.askToApplyNtpSettingImportIfNeeded(
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

        mCrossDeviceSettingImporter.askToApplyNtpSettingImportIfNeeded(
                preferencesToApply, /* onlyOmniboxPosition= */ false);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        snackbar.getController().onAction(null);

        // Verify that the "Undo" snackbar is shown.
        verify(mHomeModulesConfigManager).setPrefAllCardsEnabled(false);
        verify(mSnackbarManager, times(2)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar undoSnackbar = mSnackbarCaptor.getValue();
        assertEquals(
                mActivity.getString(R.string.synced_set_up_snackbar_applied_confirmation),
                undoSnackbar.getTextForTesting());
        assertEquals(mActivity.getString(R.string.undo), undoSnackbar.getActionText());

        // Simulate clicking the "Undo" action button.
        undoSnackbar.getController().onAction(null);

        // Verify that the preference is changed back and the "Redo" snackbar is shown.
        verify(mHomeModulesConfigManager).setPrefAllCardsEnabled(true);
        verify(mSnackbarManager, times(3)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar redoSnackbar = mSnackbarCaptor.getValue();
        assertEquals(
                mActivity.getString(R.string.synced_set_up_snackbar_removed_confirmation),
                redoSnackbar.getTextForTesting());
        assertEquals(mActivity.getString(R.string.redo), redoSnackbar.getActionText());
    }

    @Test
    public void testRedo() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false);
        when(mPrefService.isDefaultValuePreference(Pref.MAGIC_STACK_HOME_MODULE_ENABLED))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        mCrossDeviceSettingImporter.askToApplyNtpSettingImportIfNeeded(
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
                mActivity.getString(R.string.synced_set_up_snackbar_applied_confirmation),
                secondUndoSnackbar.getTextForTesting());
        assertEquals(mActivity.getString(R.string.undo), secondUndoSnackbar.getActionText());
    }

    @Test
    public void testAskToApplyNtpSettingImportIfNeeded_OmniboxOnly_differs() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        mCrossDeviceSettingImporter.askToApplyNtpSettingImportIfNeeded(
                preferencesToApply, /* onlyOmniboxPosition= */ true);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        // Simulate clicking the action button.
        snackbar.getController().onAction(null);

        // Verify that only the local state preference is changed.
        verify(mLocalPrefService).setBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        verify(mHomeModulesConfigManager, never()).setPrefAllCardsEnabled(any(Boolean.class));
        assertTrue(
                mUserActionTester
                        .getActions()
                        .contains("Android.CrossDeviceSettingImport.OmniboxPosition.Apply"));
    }

    @Test
    public void testAskToApplyNtpSettingImportIfNeeded_OmniboxOnly_noDiffs() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, true);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        mCrossDeviceSettingImporter.askToApplyNtpSettingImportIfNeeded(
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

        mCrossDeviceSettingImporter.askToApplyNtpSettingImportIfNeeded(
                preferencesToApply, /* onlyOmniboxPosition= */ false);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testRecordUma_UndoRedo() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION, false);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION)).thenReturn(true);

        mCrossDeviceSettingImporter.askToApplyNtpSettingImportIfNeeded(
                preferencesToApply, /* onlyOmniboxPosition= */ true);

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        snackbar.getController().onAction(null); // Apply

        verify(mSnackbarManager, times(2)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar undoSnackbar = mSnackbarCaptor.getValue();
        undoSnackbar.getController().onAction(null); // Undo

        assertTrue(
                mUserActionTester
                        .getActions()
                        .contains("Android.CrossDeviceSettingImport.OmniboxPosition.Undo"));

        verify(mSnackbarManager, times(3)).showSnackbar(mSnackbarCaptor.capture());
        Snackbar redoSnackbar = mSnackbarCaptor.getValue();
        redoSnackbar.getController().onAction(null); // Redo

        assertTrue(
                mUserActionTester
                        .getActions()
                        .contains("Android.CrossDeviceSettingImport.OmniboxPosition.Redo"));
    }

    @Test
    public void testTabObserverManagement() {
        // Simulate initial tab.
        mTabChangeCallbackCaptor.getValue().onResult(mTab);
        verify(mTab).addObserver(any(TabObserver.class));

        // Simulate tab change.
        mTabChangeCallbackCaptor.getValue().onResult(mTab2);
        verify(mTab).removeObserver(any(TabObserver.class));
        verify(mTab2).addObserver(any(TabObserver.class));

        // Simulate destroy.
        mCrossDeviceSettingImporter.destroy();
        verify(mTab2).removeObserver(any(TabObserver.class));
    }
}
