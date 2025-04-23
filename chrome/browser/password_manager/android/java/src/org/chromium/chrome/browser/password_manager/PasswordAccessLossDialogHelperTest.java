// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ActivityScenario;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.BuildInfo;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
import org.chromium.chrome.browser.access_loss.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Tests for {@link PasswordAccessLossDialogHelper} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
public class PasswordAccessLossDialogHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private PrefService mPrefService;
    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;
    @Mock private SettingsCustomTabLauncher mSettingsCustomTabIntentLauncher;
    @Mock private BuildInfo mBuildInfo;
    private FakeModalDialogManager mModalDialogManager;
    private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    @Before
    public void setUp() {
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(any())).thenReturn(mPrefService);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        mModalDialogManagerSupplier = new ObservableSupplierImpl<>(mModalDialogManager);
        FakePasswordManagerBackendSupportHelper helper =
                new FakePasswordManagerBackendSupportHelper();
        helper.setBackendPresent(true);
        PasswordManagerBackendSupportHelper.setInstanceForTesting(helper);
    }

    @Test
    public void testPasswordAccessLossDialogNoGmsCoreButPasswords() {
        when(mPasswordManagerUtilBridgeJniMock.getPasswordAccessLossWarningType(mPrefService))
                .thenReturn(PasswordAccessLossWarningType.NO_GMS_CORE);
        when(mPrefService.getBoolean(Pref.EMPTY_PROFILE_STORE_LOGIN_DATABASE)).thenReturn(false);
        assertThatShowAccessLossWarning(/* isTrue= */ true);

        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        Context context = RuntimeEnvironment.getApplication().getApplicationContext();
        assertEquals(
                context.getString(R.string.access_loss_no_gms_title),
                ((TextView) customView.findViewById(R.id.title)).getText());
        assertEquals(
                context.getString(R.string.access_loss_no_gms_desc),
                ((TextView) customView.findViewById(R.id.details)).getText());
        assertTrue(customView.findViewById(R.id.help_button).getVisibility() == View.VISIBLE);
        assertEquals(
                context.getString(R.string.access_loss_no_gms_positive_button_text),
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                context.getString(R.string.close),
                dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    @Test
    public void testPasswordAccessLossDialogNoUpm() {
        when(mPasswordManagerUtilBridgeJniMock.getPasswordAccessLossWarningType(mPrefService))
                .thenReturn(PasswordAccessLossWarningType.NO_UPM);
        assertThatShowAccessLossWarning(/* isTrue= */ true);

        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        Context context = RuntimeEnvironment.getApplication().getApplicationContext();
        assertEquals(
                context.getString(R.string.access_loss_update_gms_title),
                ((TextView) customView.findViewById(R.id.title)).getText());
        assertEquals(
                context.getString(R.string.access_loss_no_upm_desc),
                ((TextView) customView.findViewById(R.id.details)).getText());
        assertTrue(customView.findViewById(R.id.help_button).getVisibility() == View.VISIBLE);
        assertEquals(
                context.getString(R.string.password_manager_outdated_gms_positive_button),
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                context.getString(R.string.password_manager_outdated_gms_negative_button),
                dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    @Test
    public void testPasswordAccessLossDialogOnlyAccountUpm() {
        when(mPasswordManagerUtilBridgeJniMock.getPasswordAccessLossWarningType(mPrefService))
                .thenReturn(PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM);
        assertThatShowAccessLossWarning(/* isTrue= */ true);

        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        Context context = RuntimeEnvironment.getApplication().getApplicationContext();
        assertEquals(
                context.getString(R.string.access_loss_update_gms_title),
                ((TextView) customView.findViewById(R.id.title)).getText());
        assertEquals(
                context.getString(R.string.access_loss_update_gms_desc),
                ((TextView) customView.findViewById(R.id.details)).getText());
        assertTrue(customView.findViewById(R.id.help_button).getVisibility() == View.VISIBLE);
        assertEquals(
                context.getString(R.string.password_manager_outdated_gms_positive_button),
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                context.getString(R.string.password_manager_outdated_gms_negative_button),
                dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    @Test
    public void testPasswordAccessLossDialogNewGmsMigrationFailed() {
        when(mPasswordManagerUtilBridgeJniMock.getPasswordAccessLossWarningType(mPrefService))
                .thenReturn(PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        assertThatShowAccessLossWarning(/* isTrue= */ true);

        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        Context context = RuntimeEnvironment.getApplication().getApplicationContext();
        assertEquals(
                context.getString(R.string.access_loss_fix_problem_title),
                ((TextView) customView.findViewById(R.id.title)).getText());
        assertEquals(
                context.getString(R.string.access_loss_fix_problem_desc),
                ((TextView) customView.findViewById(R.id.details)).getText());
        assertTrue(customView.findViewById(R.id.help_button).getVisibility() == View.GONE);
        assertEquals(
                context.getString(R.string.access_loss_fix_problem_positive_button_text),
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                context.getString(R.string.password_manager_outdated_gms_negative_button),
                dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    @Test
    public void testAccessLossWarningWhenNoGmsNoPasswords() {
        when(mPasswordManagerUtilBridgeJniMock.getPasswordAccessLossWarningType(mPrefService))
                .thenReturn(PasswordAccessLossWarningType.NO_GMS_CORE);
        when(mBuildInfo.getGmsVersionCode()).thenReturn("");
        when(mPrefService.getBoolean(Pref.EMPTY_PROFILE_STORE_LOGIN_DATABASE)).thenReturn(true);
        assertThatShowAccessLossWarning(/* isTrue= */ true);

        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        View customView = dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        Context context = RuntimeEnvironment.getApplication().getApplicationContext();
        assertEquals(
                context.getString(R.string.access_loss_no_gms_no_passwords_title),
                ((TextView) customView.findViewById(R.id.title)).getText());
        assertEquals(
                context.getString(R.string.access_loss_no_gms_no_passwords_desc),
                ((TextView) customView.findViewById(R.id.details)).getText());
        assertTrue(customView.findViewById(R.id.help_button).getVisibility() == View.VISIBLE);
        assertEquals(
                context.getString(R.string.close),
                dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
    public void testAccessLossWarningTypeNoneWhenDbDeprecationEnabled() {
        when(mPasswordManagerUtilBridgeJniMock.getPasswordAccessLossWarningType(mPrefService))
                .thenReturn(PasswordAccessLossWarningType.NO_GMS_CORE);
        when(mBuildInfo.getGmsVersionCode()).thenReturn("");
        when(mPrefService.getBoolean(Pref.EMPTY_PROFILE_STORE_LOGIN_DATABASE)).thenReturn(true);
        assertThatShowAccessLossWarning(/* isTrue= */ false);
    }

    private void assertThatShowAccessLossWarning(boolean isTrue) {
        try (ActivityScenario<Activity> scenario = ActivityScenario.launch(Activity.class)) {
            scenario.onActivity(
                    activity -> {
                        boolean warningShown =
                                PasswordAccessLossDialogHelper.tryShowAccessLossWarning(
                                        mProfile,
                                        activity,
                                        ManagePasswordsReferrer.CHROME_SETTINGS,
                                        mModalDialogManagerSupplier,
                                        mSettingsCustomTabIntentLauncher,
                                        mBuildInfo);
                        boolean conditionFulfilled = isTrue ? warningShown : !warningShown;
                        assertTrue(conditionFulfilled);
                    });
        }
    }
}
