// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.ntp_cards.NtpCardsMediator.MODULE_TYPE_TO_USER_PREFS_KEY;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

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

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
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
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;

    @Captor private ArgumentCaptor<ModalDialogManagerObserver> mModalDialogManagerObserverCaptor;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;

    private Activity mActivity;
    private CrossDeviceSettingImporter mCrossDeviceSettingImporter;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        when(mModalDialogManagerSupplier.get()).thenReturn(mModalDialogManager);
        when(mSnackbarManagerSupplier.get()).thenReturn(mSnackbarManager);
        when(mActivityTabSupplier.get()).thenReturn(mTab);
        when(mTab.getProfile()).thenReturn(mProfile);

        UserPrefs.setPrefServiceForTesting(mPrefService);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);

        mCrossDeviceSettingImporter =
                new CrossDeviceSettingImporter(
                        mActivityLifecycleDispatcher,
                        mActivityTabSupplier,
                        mActivity,
                        mModalDialogManagerSupplier,
                        mSnackbarManagerSupplier);
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

        mCrossDeviceSettingImporter.askToApplyNtpSettingImportIfNeeded(preferencesToApply);

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

        mCrossDeviceSettingImporter.askToApplyNtpSettingImportIfNeeded(preferencesToApply);

        verify(mSnackbarManager, times(0)).showSnackbar(mSnackbarCaptor.capture());
    }

    @Test
    public void testUndo() {
        Map<String, Object> preferencesToApply = new HashMap<>();
        preferencesToApply.put(Pref.MAGIC_STACK_HOME_MODULE_ENABLED, false);
        when(mPrefService.isDefaultValuePreference(Pref.MAGIC_STACK_HOME_MODULE_ENABLED))
                .thenReturn(false);
        when(mPrefService.getBoolean(Pref.MAGIC_STACK_HOME_MODULE_ENABLED)).thenReturn(true);

        mCrossDeviceSettingImporter.askToApplyNtpSettingImportIfNeeded(preferencesToApply);

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

        mCrossDeviceSettingImporter.askToApplyNtpSettingImportIfNeeded(preferencesToApply);

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
}
