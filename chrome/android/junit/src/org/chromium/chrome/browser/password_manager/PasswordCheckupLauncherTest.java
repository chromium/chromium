// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;
import static org.mockito.Mockito.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.password_manager.PasswordCheckReferrer.LEAK_DIALOG;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Tests for password manager helper methods. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordCheckupLauncherTest {
    private static final AccountInfo TEST_ACCOUNT = AccountManagerTestRule.TEST_ACCOUNT_1;
    private static final String TEST_NO_EMAIL_ADDRESS = null;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Spy private Context mContext = RuntimeEnvironment.application.getApplicationContext();

    @Captor private ArgumentCaptor<Intent> mIntentCaptor;

    @Mock private Profile mProfile;

    @Mock private PrefService mPrefService;

    @Mock private UserPrefs.Natives mMockUserPrefsJni;

    @Mock private PasswordManagerUtilBridge.Natives mMockPasswordManagerUtilBridgeJni;

    @Mock private SyncService mMockSyncService;

    @Mock WindowAndroid mMockWindowAndroid;

    @Mock private PendingIntent mMockPendingIntentForLocalCheckup;

    @Mock private PendingIntent mMockPendingIntentForAccountCheckup;

    private FakePasswordManagerBackendSupportHelper mFakeBackendSupportHelper =
            new FakePasswordManagerBackendSupportHelper();

    private ModalDialogManager mModalDialogManager;

    private FakePasswordCheckupClientHelperFactoryImpl mFakePasswordCheckupClientHelperFactory =
            new FakePasswordCheckupClientHelperFactoryImpl();

    private FakePasswordCheckupClientHelper mFakePasswordCheckupClientHelper;

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            new FakeAccountManagerFacade();

    @Before
    public void setUp() throws PasswordCheckBackendException {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mMockUserPrefsJni);
        mJniMocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mMockPasswordManagerUtilBridgeJni);
        when(mMockPasswordManagerUtilBridgeJni.areMinUpmRequirementsMet()).thenReturn(true);

        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mMockUserPrefsJni.get(mProfile)).thenReturn(mPrefService);

        SyncServiceFactory.setInstanceForTesting(mMockSyncService);
        when(mMockSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mMockSyncService.isEngineInitialized()).thenReturn(true);
        when(mMockSyncService.hasSyncConsent()).thenReturn(true);

        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
        mFakeAccountManagerFacade.addAccount(TEST_ACCOUNT);
        when(mMockSyncService.getAccountInfo()).thenReturn(TEST_ACCOUNT);

        PasswordManagerBackendSupportHelper.setInstanceForTesting(mFakeBackendSupportHelper);
        mFakeBackendSupportHelper.setBackendPresent(true);

        PasswordCheckupClientHelperFactory.setFactoryForTesting(
                mFakePasswordCheckupClientHelperFactory);
        mFakePasswordCheckupClientHelper =
                (FakePasswordCheckupClientHelper)
                        mFakePasswordCheckupClientHelperFactory.createHelper();
        mFakePasswordCheckupClientHelper.setIntentForLocalCheckup(
                mMockPendingIntentForLocalCheckup);
        mFakePasswordCheckupClientHelper.setIntentForAccountCheckup(
                mMockPendingIntentForAccountCheckup);

        when(mMockWindowAndroid.getContext()).thenReturn(new WeakReference<>(mContext));
        mModalDialogManager =
                new ModalDialogManager(
                        mock(ModalDialogManager.Presenter.class),
                        ModalDialogManager.ModalDialogType.APP);
        when(mMockWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);
    }

    @Test
    public void testLaunchCheckupOnDeviceShowsPasswordCheckupForAccount()
            throws PendingIntent.CanceledException {
        when(mMockSyncService.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
        when(mMockPasswordManagerUtilBridgeJni.shouldUseUpmWiring(mMockSyncService, mPrefService))
                .thenReturn(true);

        PasswordCheckupLauncher.launchCheckupOnDevice(
                mProfile, mMockWindowAndroid, LEAK_DIALOG, TEST_ACCOUNT.getEmail());

        verify(mMockPendingIntentForAccountCheckup).send();
    }

    @Test
    public void testLaunchCheckupOnDeviceShowsPasswordCheckupForLocalWhenNotSyncing()
            throws PendingIntent.CanceledException {
        when(mMockPasswordManagerUtilBridgeJni.shouldUseUpmWiring(mMockSyncService, mPrefService))
                .thenReturn(true);

        PasswordCheckupLauncher.launchCheckupOnDevice(
                mProfile, mMockWindowAndroid, LEAK_DIALOG, TEST_NO_EMAIL_ADDRESS);

        verify(mMockPendingIntentForLocalCheckup).send();
    }

    @Test
    public void testLaunchCheckupOnDeviceShowsPasswordCheckupForLocalWhenSyncing()
            throws PendingIntent.CanceledException {
        // Local checkup will be launched from the leak detection dialog if the leaked credential is
        // stored only in the local store, even though the user is syncing passwords.
        when(mMockSyncService.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
        when(mMockPasswordManagerUtilBridgeJni.shouldUseUpmWiring(mMockSyncService, mPrefService))
                .thenReturn(true);

        PasswordCheckupLauncher.launchCheckupOnDevice(
                mProfile, mMockWindowAndroid, LEAK_DIALOG, TEST_NO_EMAIL_ADDRESS);

        verify(mMockPendingIntentForLocalCheckup).send();
    }

    @Test
    public void testLaunchPasswordCheckShowsUpdateGmsDialog()
            throws PendingIntent.CanceledException {
        when(mMockPasswordManagerUtilBridgeJni.shouldUseUpmWiring(mMockSyncService, mPrefService))
                .thenReturn(true);
        when(mMockPasswordManagerUtilBridgeJni.isGmsCoreUpdateRequired(
                        mPrefService, mMockSyncService))
                .thenReturn(true);

        PasswordCheckupLauncher.launchCheckupOnDevice(
                mProfile, mMockWindowAndroid, LEAK_DIALOG, TEST_NO_EMAIL_ADDRESS);

        verify(mMockPendingIntentForLocalCheckup, times(0)).send();
        verify(mMockPendingIntentForAccountCheckup, times(0)).send();
        PropertyModel dialogModel = mModalDialogManager.getCurrentDialogForTest();
        assertThat(
                dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1),
                is(mContext.getString(R.string.password_manager_outdated_gms_dialog_description)));
    }

    @Test
    public void testLaunchSafetyCheckOpensSafetyCheckInChromeSettings()
            throws PendingIntent.CanceledException {
        when(mMockSyncService.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.PASSWORDS));
        when(mMockPasswordManagerUtilBridgeJni.shouldUseUpmWiring(mMockSyncService, mPrefService))
                .thenReturn(true);

        PasswordCheckupLauncher.launchSafetyCheck(mMockWindowAndroid);

        verify(mContext, times(1)).startActivity(mIntentCaptor.capture(), isNull());

        Intent intent = mIntentCaptor.getValue();
        assertThat(
                intent.getExtra(SettingsActivity.EXTRA_SHOW_FRAGMENT),
                is(SafetyCheckSettingsFragment.class.getName()));
    }
}
