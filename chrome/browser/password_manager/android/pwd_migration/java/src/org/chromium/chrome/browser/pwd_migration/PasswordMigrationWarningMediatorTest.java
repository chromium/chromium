// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.PASSWORD_MIGRATION_WARNING_USER_ACTIONS;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ACCOUNT_DISPLAY_NAME;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.SHOULD_OFFER_SYNC;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;

import androidx.fragment.app.FragmentManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.password_manager.PasswordMetricsUtil;
import org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.PasswordMigrationWarningSheetStateAtClosing;
import org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.PasswordMigrationWarningUserActions;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningMediator.MigrationWarningOptionsHandler;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.MigrationOption;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountCapabilitiesConstants;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/** Tests for {@link PasswordMigrationWarningMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class PasswordMigrationWarningMediatorTest {
    private static final String TEST_EMAIL = "user@domain.com";
    private static final String FULL_NAME = "full name";
    private static final AccountInfo ACCOUNT_INFO =
            new AccountInfo(
                    new CoreAccountId("gaia-id-user"),
                    TEST_EMAIL,
                    "gaia-id-user",
                    FULL_NAME,
                    "given name",
                    null,
                    new AccountCapabilities(new HashMap<>()));
    private static final AccountInfo NON_DISPLAYABLE_EMAIL_ACCOUNT_INFO =
            new AccountInfo(
                    new CoreAccountId("gaia-id-user"),
                    TEST_EMAIL,
                    "gaia-id-user",
                    FULL_NAME,
                    "given name",
                    null,
                    new AccountCapabilities(
                            new HashMap<>(
                                    Map.of(
                                            AccountCapabilitiesConstants
                                                    .CAN_HAVE_EMAIL_ADDRESS_DISPLAYED_CAPABILITY_NAME,
                                            false))));

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    private PasswordMigrationWarningMediator mMediator;
    private PropertyModel mModel;
    @Mock private FragmentManager mFragmentManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Profile mProfile;
    @Mock private MigrationWarningOptionsHandler mOptionsHandler;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private PrefService mPrefService;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private SyncService mSyncService;
    @Mock private SigninManager mSigninManager;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJni);
        mJniMocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeJniMock);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
        mMediator =
                new PasswordMigrationWarningMediator(
                        mProfile, mOptionsHandler, PasswordMigrationWarningTriggers.CHROME_STARTUP);
        mModel =
                PasswordMigrationWarningProperties.createDefaultModel(
                        mMediator::onShown, mMediator::onDismissed, mMediator);
        mMediator.initializeModel(mModel);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
    }

    @Test
    public void testShowWarningChangesVisibility() {
        mModel.set(VISIBLE, false);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertTrue(mModel.get(VISIBLE));
    }

    @Test
    public void testShowWarningShouldOfferSyncToFalse() {
        when(mIdentityServicesProvider.getSigninManager(mProfile)).thenReturn(mSigninManager);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        mModel.set(VISIBLE, false);
        mMediator.showWarning(ScreenType.OPTIONS_SCREEN);
        assertFalse(mModel.get(SHOULD_OFFER_SYNC));
    }

    @Test
    public void testOnDismissedHidesTheSheet() {
        mModel.set(VISIBLE, true);
        mMediator.onDismissed(StateChangeReason.NONE);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testOnDismissedFromIntroScreenRecordsUserAction() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                                PasswordMigrationWarningUserActions.DISMISS_INTRODUCTION)
                        .build();

        mModel.set(VISIBLE, true);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        mMediator.onDismissed(StateChangeReason.SWIPE);

        histogram.assertExpected();
    }

    @Test
    public void testOnDismissedFromMoreOptionsScreenRecordsUserAction() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                                PasswordMigrationWarningUserActions.DISMISS_MORE_OPTIONS)
                        .build();

        mModel.set(VISIBLE, true);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        mMediator.showWarning(ScreenType.OPTIONS_SCREEN);
        mMediator.onDismissed(StateChangeReason.SWIPE);

        histogram.assertExpected();
    }

    @Test
    public void testDismissHandlerHidesTheSheet() {
        assertNotNull(mModel.get(DISMISS_HANDLER));
        mModel.set(VISIBLE, true);
        mModel.get(DISMISS_HANDLER).onResult(StateChangeReason.NONE);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testOnMoreOptionsChangesTheModel() {
        mModel.set(VISIBLE, true);
        mModel.set(CURRENT_SCREEN, ScreenType.INTRO_SCREEN);
        mMediator.onMoreOptions();
        assertEquals(mModel.get(CURRENT_SCREEN), ScreenType.OPTIONS_SCREEN);
    }

    @Test
    public void testOnMoreOptionsRecordsUserAction() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                                PasswordMigrationWarningUserActions.MORE_OPTIONS)
                        .build();

        mModel.set(VISIBLE, true);
        mModel.set(CURRENT_SCREEN, ScreenType.INTRO_SCREEN);
        mMediator.onMoreOptions();

        histogram.assertExpected();
    }

    @Test
    public void testOnAcknowledgeHidesTheSheet() {
        mModel.set(VISIBLE, true);
        mMediator.onAcknowledge(mBottomSheetController);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testOnAcknowledgeRecordsUserAction() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                                PasswordMigrationWarningUserActions.GOT_IT)
                        .build();

        mMediator.onAcknowledge(mBottomSheetController);

        histogram.assertExpected();
    }

    @Test
    public void testAcknowledgementIsSavedInPrefs() {
        mMediator.onAcknowledge(mBottomSheetController);
        verify(mPrefService)
                .setBoolean(eq(Pref.USER_ACKNOWLEDGED_LOCAL_PASSWORDS_MIGRATION_WARNING), eq(true));
    }

    @Test
    public void testOnCancelHidesTheSheet() {
        mModel.set(VISIBLE, true);
        mMediator.onCancel(mBottomSheetController);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testOnCancelRecordsUserAction() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                                PasswordMigrationWarningUserActions.CANCEL)
                        .build();

        mMediator.onCancel(mBottomSheetController);

        histogram.assertExpected();
    }

    @Test
    public void testOnNextDismissesTheSheet() {
        mModel.set(VISIBLE, true);
        mMediator.onNext(MigrationOption.SYNC_PASSWORDS, mFragmentManager);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testOnNextSelectedSyncNoConsent() {
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(false);
        mMediator.onNext(MigrationOption.SYNC_PASSWORDS, mFragmentManager);
        verify(mOptionsHandler).startSyncConsentFlow();
    }

    @Test
    public void testOnNextSelectedSyncWithConsentButPasswordsDisabled() {
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.getSelectedTypes()).thenReturn(Collections.EMPTY_SET);
        mMediator.onNext(MigrationOption.SYNC_PASSWORDS, mFragmentManager);
        verify(mOptionsHandler).openSyncSettings();
    }

    @Test
    public void testOnNextRecordsSyncUserAction() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                                PasswordMigrationWarningUserActions.SYNC)
                        .build();

        mMediator.onNext(MigrationOption.SYNC_PASSWORDS, mFragmentManager);

        histogram.assertExpected();
    }

    @Test
    public void testOnNextRecordsExportUserAction() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                PASSWORD_MIGRATION_WARNING_USER_ACTIONS,
                                PasswordMigrationWarningUserActions.EXPORT)
                        .build();

        mMediator.onNext(MigrationOption.EXPORT_AND_DELETE, mFragmentManager);

        histogram.assertExpected();
    }

    @Test
    public void testOnNextWithoutPasswordsWithExportingSelectedStartsTheExportFlow() {
        mMediator.onNext(MigrationOption.EXPORT_AND_DELETE, mFragmentManager);

        verify(mOptionsHandler).startExportFlow(mFragmentManager);
    }

    @Test
    public void testOnNextWithPasswordsWithExportingSelectedStartsTheExportFlow() {
        mMediator.passwordListAvailable(2);
        mMediator.onNext(MigrationOption.EXPORT_AND_DELETE, mFragmentManager);

        verify(mOptionsHandler).startExportFlow(mFragmentManager);
    }

    @Test
    public void testGetAccountDisplayNameReturnsEmail() {
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL, "0"));
        when(mIdentityManager.findExtendedAccountInfoByEmailAddress(TEST_EMAIL))
                .thenReturn(ACCOUNT_INFO);

        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertEquals(TEST_EMAIL, mModel.get(ACCOUNT_DISPLAY_NAME));
    }

    @Test
    public void testGetAccountDisplayNameReturnsFullName() {
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL, "0"));
        when(mIdentityManager.findExtendedAccountInfoByEmailAddress(TEST_EMAIL))
                .thenReturn(NON_DISPLAYABLE_EMAIL_ACCOUNT_INFO);

        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertEquals(FULL_NAME, mModel.get(ACCOUNT_DISPLAY_NAME));
    }

    @Test
    public void testGetAccountDisplayNameWithNoSignedInUser() {
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)).thenReturn(null);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);

        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertEquals(null, mModel.get(ACCOUNT_DISPLAY_NAME));
    }

    @Test
    public void testNotifyTheOptionsHandlerWhenPasswordsAreAvailable() {
        mMediator.passwordListAvailable(2);

        verify(mOptionsHandler).passwordsAvailable();
    }

    @Test
    public void testSyncNotOfferedIfPolicyDisabledSignIn() {
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)).thenReturn(null);
        when(mIdentityServicesProvider.getSigninManager(mProfile)).thenReturn(mSigninManager);
        when(mSigninManager.isSigninDisabledByPolicy()).thenReturn(true);
        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertFalse(mModel.get(SHOULD_OFFER_SYNC));
    }

    @Test
    public void testSyncNotOfferedIfPolicyDisabledSync() {
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)).thenReturn(null);
        when(mIdentityServicesProvider.getSigninManager(mProfile)).thenReturn(mSigninManager);
        when(mSigninManager.isSigninDisabledByPolicy()).thenReturn(false);
        when(mSyncService.isSyncDisabledByEnterprisePolicy()).thenReturn(true);

        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertFalse(mModel.get(SHOULD_OFFER_SYNC));
    }

    @Test
    public void testSyncNotOfferedIfPolicyDisabledPasswordsSync() {
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)).thenReturn(null);
        when(mIdentityServicesProvider.getSigninManager(mProfile)).thenReturn(mSigninManager);
        when(mSigninManager.isSigninDisabledByPolicy()).thenReturn(false);
        when(mSyncService.isSyncDisabledByEnterprisePolicy()).thenReturn(false);
        when(mSyncService.isTypeManagedByPolicy(UserSelectableType.PASSWORDS)).thenReturn(true);

        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertFalse(mModel.get(SHOULD_OFFER_SYNC));
    }

    @Test
    public void testOnShownSetsPreForStartup() {
        mMediator.onShown();
        verify(mPrefService)
                .setBoolean(Pref.LOCAL_PASSWORD_MIGRATION_WARNING_SHOWN_AT_STARTUP, true);
    }

    @Test
    public void testOnShownDoesntSetPrefIfNotOnStartup() {
        PasswordMigrationWarningMediator mediator =
                new PasswordMigrationWarningMediator(
                        mProfile, mOptionsHandler, PasswordMigrationWarningTriggers.TOUCH_TO_FILL);
        mediator.onShown();
        verify(mPrefService, never())
                .setBoolean(Pref.LOCAL_PASSWORD_MIGRATION_WARNING_SHOWN_AT_STARTUP, true);
    }

    @Test
    public void testSheetClosedResetsTimestampWhenNoFragment() {
        mMediator.onSheetClosed(StateChangeReason.NONE, false);

        verify(mPrefService).setString(Pref.LOCAL_PASSWORDS_MIGRATION_WARNING_SHOWN_TIMESTAMP, "0");
    }

    @Test
    public void testSheetClosedDoesntResetTimestampWhenFragmentSet() {
        mMediator.onSheetClosed(StateChangeReason.NONE, true);

        verify(mPrefService, never())
                .setString(Pref.LOCAL_PASSWORDS_MIGRATION_WARNING_SHOWN_TIMESTAMP, "0");
    }

    @Test
    public void testRecordEmptySheetTriggerWhenFragmentWasNeverSet() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                PasswordMetricsUtil.PASSWORD_MIGRATION_WARNING_EMPTY_SHEET_TRIGGER,
                                PasswordMigrationWarningTriggers.CHROME_STARTUP)
                        .build();

        mMediator.onSheetClosed(StateChangeReason.NONE, false);

        histogram.assertExpected();
    }

    @Test
    public void testDontRecordEmptySheetTriggerWhenFragmentWasSet() {
        var histogram = HistogramWatcher.newBuilder().build();

        mMediator.onSheetClosed(StateChangeReason.NONE, true);

        histogram.assertExpected();
    }

    @Test
    public void testRecordEmptySheetClosedWithoutUserInteraction() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                PasswordMetricsUtil
                                        .PASSWORD_MIGRATION_WARNING_SHEET_STATE_AT_CLOSING,
                                PasswordMigrationWarningSheetStateAtClosing
                                        .EMPTY_SHEET_CLOSED_WITHOUT_USER_INTERACTION)
                        .build();

        mMediator.onSheetClosed(StateChangeReason.NONE, false);

        histogram.assertExpected();
    }

    @Test
    public void testRecordEmptySheetClosedByUserInteraction() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                PasswordMetricsUtil
                                        .PASSWORD_MIGRATION_WARNING_SHEET_STATE_AT_CLOSING,
                                PasswordMigrationWarningSheetStateAtClosing
                                        .EMPTY_SHEET_CLOSED_BY_USER_INTERACTION)
                        .build();

        mMediator.onSheetClosed(StateChangeReason.BACK_PRESS, false);

        histogram.assertExpected();
    }

    @Test
    public void testRecordClosingTheSheetWithFullContent() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                PasswordMetricsUtil
                                        .PASSWORD_MIGRATION_WARNING_SHEET_STATE_AT_CLOSING,
                                PasswordMigrationWarningSheetStateAtClosing.FULL_SHEET_CLOSED)
                        .build();

        mMediator.onSheetClosed(StateChangeReason.BACK_PRESS, true);

        histogram.assertExpected();
    }
}
