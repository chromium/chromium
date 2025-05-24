// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.widget.ToastManager;

/**
 * Tests {@link SigninAndHistorySyncActivityLauncherImpl}.
 *
 * <p>TODO(crbug.com/354912290): Update this test when the error UI will be implemented.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowToast.class})
public class SigninAndHistorySyncActivityLauncherImplTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private static final AccountPickerBottomSheetStrings BOTTOM_SHEET_STRINGS =
            new AccountPickerBottomSheetStrings.Builder(
                            R.string.signin_account_picker_bottom_sheet_title)
                    .build();
    private static final FullscreenSigninAndHistorySyncConfig FULLSCREEN_CONFIG =
            new FullscreenSigninAndHistorySyncConfig.Builder().build();
    private static final BottomSheetSigninAndHistorySyncConfig BOTTOM_SHEET_CONFIG =
            new BottomSheetSigninAndHistorySyncConfig.Builder(
                            BOTTOM_SHEET_STRINGS,
                            NoAccountSigninMode.BOTTOM_SHEET,
                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                            HistorySyncConfig.OptInMode.REQUIRED)
                    .build();

    private final Context mContext = ContextUtils.getApplicationContext();
    @Mock private IdentityServicesProvider mIdentityProviderMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private Profile mProfileMock;
    @Mock private HistorySyncHelper mHistorySyncHelperMock;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private PrefService mPrefService;

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mIdentityProviderMock);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelperMock);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        lenient().when(mUserPrefsJni.get(any())).thenReturn(mPrefService);
    }

    @After
    public void tearDown() {
        ShadowToast.reset();
        ToastManager.resetForTesting();
    }

    @Test
    @MediumTest
    public void testCreateBottomSheetSigninIntentOrShowError() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContext,
                                            mProfileMock,
                                            BOTTOM_SHEET_CONFIG,
                                            SigninAccessPoint.RECENT_TABS);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateBottomSheetSigninIntentOrShowError_withAccountId() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            BOTTOM_SHEET_STRINGS,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.REQUIRED)
                                    .selectedCoreAccountId(TestAccounts.ACCOUNT1.getId())
                                    .build();
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContext,
                                            mProfileMock,
                                            config,
                                            SigninAccessPoint.RECENT_TABS);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateBottomSheetSigninIntentOrShowError_signedInHistorySyncAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContext,
                                            mProfileMock,
                                            BOTTOM_SHEET_CONFIG,
                                            SigninAccessPoint.RECENT_TABS);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateBottomSheetSigninIntentOrShowError_signinNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mPrefService.isManagedPreference(Pref.SIGNIN_ALLOWED)).thenReturn(false);
        when(mIdentityManagerMock.hasPrimaryAccount(anyInt())).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContext,
                                            mProfileMock,
                                            BOTTOM_SHEET_CONFIG,
                                            SigninAccessPoint.RECENT_TABS);
                    assertNull(intent);
                });
        verifyToastShown(R.string.signin_account_picker_bottom_sheet_error_title);
    }

    @Test
    @MediumTest
    public void testCreateBottomSheetSigninIntentOrShowError_signinDisabledByPolicy() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mPrefService.isManagedPreference(Pref.SIGNIN_ALLOWED)).thenReturn(true);
        HistogramWatcher watchSigninDisabledToastShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninDisabledNotificationShown", SigninAccessPoint.RECENT_TABS);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .createBottomSheetSigninIntentOrShowError(
                                    mContext,
                                    mProfileMock,
                                    BOTTOM_SHEET_CONFIG,
                                    SigninAccessPoint.RECENT_TABS);
                });

        verifyToastShown(R.string.managed_by_your_organization);
        watchSigninDisabledToastShownHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testCreateBottomSheetSigninIntentOrShowError_signedInAndHistorySyncSuppressed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContext,
                                            mProfileMock,
                                            BOTTOM_SHEET_CONFIG,
                                            SigninAccessPoint.RECENT_TABS);
                    assertNull(intent);
                });
        verifyToastShown(R.string.signin_account_picker_bottom_sheet_error_title);
    }

    @Test
    @MediumTest
    public void testCreateBottomSheetSigninIntentOrShowError_signedInAndNoHistorySync() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            BOTTOM_SHEET_STRINGS,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.NONE)
                                    .build();
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContext,
                                            mProfileMock,
                                            config,
                                            SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                    assertNull(intent);
                });
        verifyToastShown(R.string.signin_account_picker_bottom_sheet_error_title);
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntent() {
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntent(
                                            mContext,
                                            mProfileMock,
                                            FULLSCREEN_CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntent_signinNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(false);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntent(
                                            mContext,
                                            mProfileMock,
                                            FULLSCREEN_CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntent_alreadySignedIn() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntent(
                                            mContext,
                                            mProfileMock,
                                            FULLSCREEN_CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntent_signedInAndHistorySyncNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntent(
                                            mContext,
                                            mProfileMock,
                                            FULLSCREEN_CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntent_signedInAndHistorySyncDeclinedOften() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntent(
                                            mContext,
                                            mProfileMock,
                                            FULLSCREEN_CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentOrShowError() {
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntentOrShowError(
                                            mContext,
                                            mProfileMock,
                                            FULLSCREEN_CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentOrShowError_signinNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(false);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntentOrShowError(
                                            mContext,
                                            mProfileMock,
                                            FULLSCREEN_CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNull(intent);
                });
        verifyToastShown(R.string.signin_account_picker_bottom_sheet_error_title);
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentOrShowError_alreadySignedIn() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntentOrShowError(
                                            mContext,
                                            mProfileMock,
                                            FULLSCREEN_CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentOrShowError_signedInAndHistorySyncNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntentOrShowError(
                                            mContext,
                                            mProfileMock,
                                            FULLSCREEN_CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNull(intent);
                });
        verifyToastShown(R.string.signin_account_picker_bottom_sheet_error_title);
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentOrShowError_signedInAndHistorySyncDeclinedOften() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntentOrShowError(
                                            mContext,
                                            mProfileMock,
                                            FULLSCREEN_CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNull(intent);
                });
        verifyToastShown(R.string.signin_account_picker_bottom_sheet_error_title);
    }

    private void verifyToastShown(@StringRes int stringId) {
        assertTrue(
                "Toast is not as expected",
                ShadowToast.showedCustomToast(
                        ContextUtils.getApplicationContext().getString(stringId), R.id.toast_text));
    }
}
