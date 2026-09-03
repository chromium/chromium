// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.Robolectric;

import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsInTab;
import org.chromium.chrome.browser.signin.services.AccountPreviewDataService;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinatorSupplier;
import org.chromium.chrome.browser.ui.signin.DelegateContext;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.SigninDelegateContext;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.ExternalEntryPoint;
import org.chromium.components.signin.base.SigninDeepLinkPayload;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.CrossDeviceInitialState;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.Collection;

/** JUnit tests for the class {@link SigninBridge}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class SigninBridgeTest {
    private static final String TEST_EXTENSION_NAME = "Test Extension";

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(
                new Object[][] {
                    {/* isWebSignin= */ true, SigninAccessPoint.WEB_SIGNIN, ""},
                    {/* isWebSignin= */ false, SigninAccessPoint.EXTENSIONS, TEST_EXTENSION_NAME}
                });
    }

    private GURL mContinueUrl;
    private static final @TabId int TAB_ID = 1;

    @Rule(order = -2)
    public final BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Parameter public boolean mIsWebSignin;

    @Parameter(1)
    public @SigninAccessPoint int mSigninAccessPoint;

    @Parameter(2)
    public @Nullable String mExtensionName;

    @Mock private Tab mTabMock;

    @Mock private Profile mProfileMock;

    @Mock private WindowAndroid mWindowAndroidMock;

    @Mock private SigninManager mSigninManagerMock;

    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsJniMock;

    @Mock private BottomSheetController mBottomSheetControllerMock;

    @Mock private BottomSheetSigninAndHistorySyncCoordinator mCoordinatorMock;

    @Mock private SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncherMock;

    @Mock
    private SigninBridge.AccountPickerBottomSheetCoordinatorFactory
            mAccountPickerBottomSheetCoordinatorFactoryMock;

    @Mock private AccountPreviewDataService mAccountPreviewDataServiceMock;
    @Mock private ModalDialogManager mModalDialogManagerMock;

    private final SettableMonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator>
            mWebSigninAndHistorySyncCoordinatorSupplier = ObservableSuppliers.createMonotonic();

    @Before
    public void setUp() {
        mContinueUrl = new GURL("https://test-continue-url.com");
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetControllerMock);
        Context context = ApplicationProvider.getApplicationContext();

        lenient().when(mWindowAndroidMock.getContext()).thenReturn(new WeakReference<>(context));
        lenient()
                .when(mWindowAndroidMock.getModalDialogManager())
                .thenReturn(mModalDialogManagerMock);
        lenient().when(mTabMock.getProfile()).thenReturn(mProfileMock);
        lenient().when(mTabMock.getWindowAndroid()).thenReturn(mWindowAndroidMock);
        lenient().when(mTabMock.isUserInteractable()).thenReturn(true);
        lenient().when(mTabMock.getId()).thenReturn(TAB_ID);
        lenient().when(mProfileMock.getOriginalProfile()).thenReturn(mProfileMock);

        IdentityServicesProvider.setSigninManagerForTesting(mSigninManagerMock);
        IdentityServicesProvider.setAccountPreviewDataServiceForTesting(
                mAccountPreviewDataServiceMock);
        SigninMetricsUtilsJni.setInstanceForTesting(mSigninMetricsUtilsJniMock);
        BottomSheetSigninAndHistorySyncCoordinatorSupplier.setInstanceForTesting(
                mWebSigninAndHistorySyncCoordinatorSupplier);
        mWebSigninAndHistorySyncCoordinatorSupplier.set(mCoordinatorMock);
    }

    @After
    public void tearDown() {
        SigninPreferencesManager.getInstance().clearWebSigninAccountPickerActiveDismissalCount();
        mWebSigninAndHistorySyncCoordinatorSupplier.destroy();
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenNoWindow() {
        //  Reset default values configured in `setUp`.
        Mockito.reset(mTabMock);
        when(mTabMock.getWindowAndroid()).thenReturn(null);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                mContinueUrl,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT1.getId(),
                mExtensionName);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        anyBoolean(),
                        anyInt(),
                        eq(TestAccounts.ACCOUNT1.getId()));
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenTabNotInteractable() {
        //  Reset default values configured in `setUp`.
        Mockito.reset(mTabMock);
        when(mTabMock.getWindowAndroid()).thenReturn(mWindowAndroidMock);
        when(mTabMock.isUserInteractable()).thenReturn(false);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                mContinueUrl,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT1.getId(),
                mExtensionName);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(mIsWebSignin),
                        eq(mSigninAccessPoint),
                        eq(TestAccounts.ACCOUNT1.getId()));
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenSigninNotAllowed() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                mContinueUrl,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT1.getId(),
                mExtensionName);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED,
                        mSigninAccessPoint);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(mIsWebSignin),
                        eq(mSigninAccessPoint),
                        eq(TestAccounts.ACCOUNT1.getId()));
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenNoAccountsOnDevice() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                mContinueUrl,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT1.getId(),
                mExtensionName);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.SUPPRESSED_NO_ACCOUNTS, mSigninAccessPoint);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(mIsWebSignin),
                        eq(mSigninAccessPoint),
                        eq(TestAccounts.ACCOUNT1.getId()));
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedIfDismissLimitReached() {
        Assume.assumeTrue(mIsWebSignin);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT,
                        SigninBridge.ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock, mContinueUrl, mAccountPickerBottomSheetCoordinatorFactoryMock, null, "");

        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.SUPPRESSED_CONSECUTIVE_DISMISSALS,
                        mSigninAccessPoint);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(true),
                        eq(SigninAccessPoint.WEB_SIGNIN),
                        eq(null));
    }

    @Test
    @SmallTest
    @DisableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testAccountPickerHasNoLimitIfAccountIsSpecified_legacy() {
        Assume.assumeTrue(mIsWebSignin);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT,
                        SigninBridge.ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                mContinueUrl,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT2.getId(),
                "");

        verify(mSigninMetricsUtilsJniMock, never())
                .logAccountConsistencyPromoAction(
                        eq(AccountConsistencyPromoAction.SUPPRESSED_CONSECUTIVE_DISMISSALS),
                        anyInt());
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock)
                .create(
                        eq(mWindowAndroidMock),
                        any(),
                        any(),
                        any(),
                        any(),
                        eq(mBottomSheetControllerMock),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(true),
                        eq(SigninAccessPoint.WEB_SIGNIN),
                        eq(TestAccounts.ACCOUNT2.getId()));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testAccountPickerHasNoLimitIfAccountIsSpecified() {
        Assume.assumeTrue(mIsWebSignin);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT,
                        SigninBridge.ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                mContinueUrl,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT2.getId(),
                mExtensionName);

        verify(mSigninMetricsUtilsJniMock, never())
                .logAccountConsistencyPromoAction(
                        eq(AccountConsistencyPromoAction.SUPPRESSED_CONSECUTIVE_DISMISSALS),
                        anyInt());
        verifyNoInteractions(mAccountPickerBottomSheetCoordinatorFactoryMock);
        verifyBottomSheetStartSigninFlow(TestAccounts.ACCOUNT2.getId());
    }

    @Test
    @SmallTest
    @DisableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testAccountPickerShown_legacy() {
        Assume.assumeTrue(mIsWebSignin);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                mContinueUrl,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT1.getId(),
                "");
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock)
                .create(
                        eq(mWindowAndroidMock),
                        any(),
                        any(),
                        any(),
                        any(),
                        eq(mBottomSheetControllerMock),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(true),
                        eq(SigninAccessPoint.WEB_SIGNIN),
                        eq(TestAccounts.ACCOUNT1.getId()));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testAccountPickerShown() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                mContinueUrl,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT1.getId(),
                mExtensionName);

        verifyNoInteractions(mAccountPickerBottomSheetCoordinatorFactoryMock);
        verifyBottomSheetStartSigninFlow(TestAccounts.ACCOUNT1.getId());
    }

    @Test
    @SmallTest
    @DisableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testAccountPickerShownWithNoSelectedAccountId_legacy() {
        Assume.assumeTrue(mIsWebSignin);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock, mContinueUrl, mAccountPickerBottomSheetCoordinatorFactoryMock, null, "");
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock)
                .create(
                        eq(mWindowAndroidMock),
                        any(),
                        any(),
                        any(),
                        any(),
                        eq(mBottomSheetControllerMock),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(true),
                        eq(SigninAccessPoint.WEB_SIGNIN),
                        isNull());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testAccountPickerShownWithNoSelectedAccountId() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                mContinueUrl,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                null,
                mExtensionName);

        verifyNoInteractions(mAccountPickerBottomSheetCoordinatorFactoryMock);
        verifyBottomSheetStartSigninFlow(/* accountId= */ null);
    }

    @Test
    @SmallTest
    @EnableFeatures(SigninFeatures.ENABLE_ADD_SESSION_REDIRECT)
    @DisableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testBottomSheetInvokedAfterAddAccountFlow_legacy() {
        Assume.assumeTrue(mIsWebSignin);
        ArgumentCaptor<WindowAndroid.IntentCallback> intentCaptor =
                ArgumentCaptor.forClass(WindowAndroid.IntentCallback.class);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        when(mWindowAndroidMock.showIntent(any(Intent.class), intentCaptor.capture(), any()))
                .thenReturn(true);
        FakeAccountManagerFacade fakeAccountManagerFacade =
                (FakeAccountManagerFacade) AccountManagerFacadeProvider.getInstance();
        AccountManagerFacadeProvider.setInstanceForTests(fakeAccountManagerFacade);
        fakeAccountManagerFacade.setAddAccountFlowResult(TestAccounts.ACCOUNT2);

        SigninBridge.startAddAccountFlow(
                mTabMock,
                TestAccounts.ACCOUNT2.getEmail(),
                mContinueUrl,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                "");

        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        intentCaptor.getValue().onIntentCompleted(Activity.RESULT_OK, null);

        verify(mAccountPickerBottomSheetCoordinatorFactoryMock)
                .create(
                        eq(mWindowAndroidMock),
                        any(),
                        any(),
                        any(),
                        any(),
                        eq(mBottomSheetControllerMock),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(true),
                        eq(SigninAccessPoint.WEB_SIGNIN),
                        eq(TestAccounts.ACCOUNT2.getId()));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        SigninFeatures.ENABLE_ADD_SESSION_REDIRECT,
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
        SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
    })
    public void testBottomSheetInvokedAfterAddAccountFlow() {
        ArgumentCaptor<WindowAndroid.IntentCallback> intentCaptor =
                ArgumentCaptor.forClass(WindowAndroid.IntentCallback.class);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        when(mWindowAndroidMock.showIntent(any(Intent.class), intentCaptor.capture(), any()))
                .thenReturn(true);

        SigninBridge.startAddAccountFlow(
                mTabMock,
                TestAccounts.ACCOUNT2.getEmail(),
                mContinueUrl,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                mExtensionName);

        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        intentCaptor.getValue().onIntentCompleted(Activity.RESULT_OK, null);

        verifyNoInteractions(mAccountPickerBottomSheetCoordinatorFactoryMock);
        verifyBottomSheetStartSigninFlow(TestAccounts.ACCOUNT2.getId());
    }

    @Test
    @SmallTest
    public void testSigninDeepLinkFlow_userSignedOut_targetAccountNotOnDevice() {
        Context context = Robolectric.buildActivity(Activity.class).get();
        lenient().when(mWindowAndroidMock.getContext()).thenReturn(new WeakReference<>(context));
        lenient().when(mProfileMock.getOriginalProfile()).thenReturn(mProfileMock);

        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(
                mSigninAndHistorySyncActivityLauncherMock);

        var payload =
                new SigninDeepLinkPayload(
                        /* externalEntryPoint= */ ExternalEntryPoint.DESKTOP_DEFAULT,
                        /* email= */ TestAccounts.ACCOUNT1.getEmail());

        var expectedConfig =
                FullscreenSigninAndHistorySyncConfig.builder(
                                context.getString(R.string.signin_deep_link_flow_signin_title),
                                context.getString(R.string.signin_deep_link_flow_signin_subtitle),
                                context.getString(
                                        R.string.signin_deep_link_flow_signin_dismiss_button),
                                context.getString(R.string.history_sync_title),
                                context.getString(R.string.history_sync_subtitle))
                        .historyOptInMode(HistorySyncConfig.OptInMode.NONE)
                        .selectedAccountEmail(TestAccounts.ACCOUNT1.getEmail())
                        .build();
        when(mSigninAndHistorySyncActivityLauncherMock.createFullscreenSigninIntentOrShowError(
                        any(), any(), any(), anyInt()))
                .thenReturn(new Intent());

        SigninBridge.startSigninDeepLinkFlow(mWindowAndroidMock, mProfileMock, payload);

        verify(mSigninAndHistorySyncActivityLauncherMock)
                .createFullscreenSigninIntentOrShowError(
                        eq(context),
                        eq(mProfileMock),
                        eq(expectedConfig),
                        eq(SigninAccessPoint.DEEP_LINK_DEFAULT));
        verify(mSigninMetricsUtilsJniMock)
                .recordCrossDeviceInitialState(
                        eq(ExternalEntryPoint.DESKTOP_DEFAULT),
                        eq(CrossDeviceInitialState.SIGNED_OUT_TARGET_ACCOUNT_NOT_ON_DEVICE));
    }

    @Test
    @SmallTest
    public void
            testSigninDeepLinkFlow_userSignedIn_toDifferentAccount_targetAccountNotOnTheDevice() {
        Context context = Robolectric.buildActivity(Activity.class).get();
        lenient().when(mWindowAndroidMock.getContext()).thenReturn(new WeakReference<>(context));
        lenient().when(mProfileMock.getOriginalProfile()).thenReturn(mProfileMock);

        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(
                mSigninAndHistorySyncActivityLauncherMock);

        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);

        var payload =
                new SigninDeepLinkPayload(
                        /* externalEntryPoint= */ ExternalEntryPoint.DESKTOP_DEFAULT,
                        /* email= */ TestAccounts.ACCOUNT2.getEmail());

        var expectedConfig =
                FullscreenSigninAndHistorySyncConfig.builderForSwitchAccountFlow(
                                context.getString(
                                        R.string.signin_deep_link_flow_switch_account_title),
                                context.getString(
                                        R.string.signin_deep_link_flow_switch_account_subtitle,
                                        TestAccounts.ACCOUNT1.getEmail(),
                                        TestAccounts.ACCOUNT2.getEmail()),
                                context.getString(
                                        R.string
                                                .signin_deep_link_flow_switch_account_dismiss_button),
                                context.getString(R.string.history_sync_title),
                                context.getString(R.string.history_sync_subtitle),
                                TestAccounts.ACCOUNT2.getEmail())
                        .historyOptInMode(HistorySyncConfig.OptInMode.NONE)
                        .build();
        when(mSigninAndHistorySyncActivityLauncherMock.createFullscreenSigninIntentOrShowError(
                        any(), any(), any(), anyInt()))
                .thenReturn(new Intent());

        SigninBridge.startSigninDeepLinkFlow(mWindowAndroidMock, mProfileMock, payload);

        verify(mSigninAndHistorySyncActivityLauncherMock)
                .createFullscreenSigninIntentOrShowError(
                        eq(context),
                        eq(mProfileMock),
                        eq(expectedConfig),
                        eq(SigninAccessPoint.DEEP_LINK_DEFAULT));
        verify(mSigninMetricsUtilsJniMock)
                .recordCrossDeviceInitialState(
                        eq(ExternalEntryPoint.DESKTOP_DEFAULT),
                        eq(
                                CrossDeviceInitialState
                                        .SIGNED_IN_WITH_DIFFERENT_ACCOUNT_TARGET_ACCOUNT_NOT_ON_DEVICE));
    }

    @Test
    @SmallTest
    public void testSigninDeepLinkFlow_userSignedIn_withTheTargetAccount() {
        Context context = Robolectric.buildActivity(Activity.class).get();
        lenient().when(mWindowAndroidMock.getContext()).thenReturn(new WeakReference<>(context));
        lenient().when(mProfileMock.getOriginalProfile()).thenReturn(mProfileMock);

        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(
                mSigninAndHistorySyncActivityLauncherMock);

        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);

        var payload =
                new SigninDeepLinkPayload(
                        /* externalEntryPoint= */ ExternalEntryPoint.DESKTOP_DEFAULT,
                        /* email= */ TestAccounts.ACCOUNT1.getEmail());

        SigninBridge.startSigninDeepLinkFlow(mWindowAndroidMock, mProfileMock, payload);

        verifyNoInteractions(mSigninAndHistorySyncActivityLauncherMock);
        verify(mSigninMetricsUtilsJniMock)
                .recordCrossDeviceInitialState(
                        eq(ExternalEntryPoint.DESKTOP_DEFAULT),
                        eq(CrossDeviceInitialState.SIGNED_IN_WITH_TARGET_ACCOUNT));
    }

    @Test
    @SmallTest
    public void testSigninDeepLinkFlow_launcherReturnsNull_recordsFlowForbidden() {
        Context context = Robolectric.buildActivity(Activity.class).get();
        lenient().when(mWindowAndroidMock.getContext()).thenReturn(new WeakReference<>(context));
        lenient().when(mProfileMock.getOriginalProfile()).thenReturn(mProfileMock);

        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(
                mSigninAndHistorySyncActivityLauncherMock);

        var payload =
                new SigninDeepLinkPayload(
                        /* externalEntryPoint= */ ExternalEntryPoint.DESKTOP_DEFAULT,
                        /* email= */ TestAccounts.ACCOUNT1.getEmail());

        var expectedConfig =
                FullscreenSigninAndHistorySyncConfig.builder(
                                context.getString(R.string.signin_deep_link_flow_signin_title),
                                context.getString(R.string.signin_deep_link_flow_signin_subtitle),
                                context.getString(
                                        R.string.signin_deep_link_flow_signin_dismiss_button),
                                context.getString(R.string.history_sync_title),
                                context.getString(R.string.history_sync_subtitle))
                        .historyOptInMode(HistorySyncConfig.OptInMode.NONE)
                        .selectedAccountEmail(TestAccounts.ACCOUNT1.getEmail())
                        .build();
        when(mSigninAndHistorySyncActivityLauncherMock.createFullscreenSigninIntentOrShowError(
                        any(), any(), any(), anyInt()))
                .thenReturn(null);

        SigninBridge.startSigninDeepLinkFlow(mWindowAndroidMock, mProfileMock, payload);

        verify(mSigninAndHistorySyncActivityLauncherMock)
                .createFullscreenSigninIntentOrShowError(
                        eq(context),
                        eq(mProfileMock),
                        eq(expectedConfig),
                        eq(SigninAccessPoint.DEEP_LINK_DEFAULT));
        verify(mSigninMetricsUtilsJniMock)
                .recordCrossDeviceInitialState(
                        eq(ExternalEntryPoint.DESKTOP_DEFAULT),
                        eq(CrossDeviceInitialState.FLOW_FORBIDDEN));
    }

    @Test
    @SmallTest
    public void testSigninDeepLinkFlow_userSignedOut_targetAccountOnDevice() {
        Context context = Robolectric.buildActivity(Activity.class).get();
        lenient().when(mWindowAndroidMock.getContext()).thenReturn(new WeakReference<>(context));
        lenient().when(mProfileMock.getOriginalProfile()).thenReturn(mProfileMock);

        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(
                mSigninAndHistorySyncActivityLauncherMock);

        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        var payload =
                new SigninDeepLinkPayload(
                        /* externalEntryPoint= */ ExternalEntryPoint.DESKTOP_DEFAULT,
                        /* email= */ TestAccounts.ACCOUNT1.getEmail());

        var expectedConfig =
                FullscreenSigninAndHistorySyncConfig.builder(
                                context.getString(R.string.signin_deep_link_flow_signin_title),
                                context.getString(R.string.signin_deep_link_flow_signin_subtitle),
                                context.getString(
                                        R.string.signin_deep_link_flow_signin_dismiss_button),
                                context.getString(R.string.history_sync_title),
                                context.getString(R.string.history_sync_subtitle))
                        .historyOptInMode(HistorySyncConfig.OptInMode.NONE)
                        .selectedAccountEmail(TestAccounts.ACCOUNT1.getEmail())
                        .build();
        when(mSigninAndHistorySyncActivityLauncherMock.createFullscreenSigninIntentOrShowError(
                        any(), any(), any(), anyInt()))
                .thenReturn(new Intent());

        SigninBridge.startSigninDeepLinkFlow(mWindowAndroidMock, mProfileMock, payload);

        verify(mSigninAndHistorySyncActivityLauncherMock)
                .createFullscreenSigninIntentOrShowError(
                        eq(context),
                        eq(mProfileMock),
                        eq(expectedConfig),
                        eq(SigninAccessPoint.DEEP_LINK_DEFAULT));
        verify(mSigninMetricsUtilsJniMock)
                .recordCrossDeviceInitialState(
                        eq(ExternalEntryPoint.DESKTOP_DEFAULT),
                        eq(CrossDeviceInitialState.SIGNED_OUT_TARGET_ACCOUNT_ON_DEVICE));
    }

    @Test
    @SmallTest
    public void testSigninDeepLinkFlow_userSignedIn_toDifferentAccount_targetAccountOnTheDevice() {
        Context context = Robolectric.buildActivity(Activity.class).get();
        lenient().when(mWindowAndroidMock.getContext()).thenReturn(new WeakReference<>(context));
        lenient().when(mProfileMock.getOriginalProfile()).thenReturn(mProfileMock);

        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(
                mSigninAndHistorySyncActivityLauncherMock);

        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);

        var payload =
                new SigninDeepLinkPayload(
                        /* externalEntryPoint= */ ExternalEntryPoint.DESKTOP_DEFAULT,
                        /* email= */ TestAccounts.ACCOUNT2.getEmail());

        var expectedConfig =
                FullscreenSigninAndHistorySyncConfig.builderForSwitchAccountFlow(
                                context.getString(
                                        R.string.signin_deep_link_flow_switch_account_title),
                                context.getString(
                                        R.string.signin_deep_link_flow_switch_account_subtitle,
                                        TestAccounts.ACCOUNT1.getEmail(),
                                        TestAccounts.ACCOUNT2.getEmail()),
                                context.getString(
                                        R.string
                                                .signin_deep_link_flow_switch_account_dismiss_button),
                                context.getString(R.string.history_sync_title),
                                context.getString(R.string.history_sync_subtitle),
                                TestAccounts.ACCOUNT2.getEmail())
                        .historyOptInMode(HistorySyncConfig.OptInMode.NONE)
                        .build();
        when(mSigninAndHistorySyncActivityLauncherMock.createFullscreenSigninIntentOrShowError(
                        any(), any(), any(), anyInt()))
                .thenReturn(new Intent());

        SigninBridge.startSigninDeepLinkFlow(mWindowAndroidMock, mProfileMock, payload);

        verify(mSigninAndHistorySyncActivityLauncherMock)
                .createFullscreenSigninIntentOrShowError(
                        eq(context),
                        eq(mProfileMock),
                        eq(expectedConfig),
                        eq(SigninAccessPoint.DEEP_LINK_DEFAULT));
        verify(mSigninMetricsUtilsJniMock)
                .recordCrossDeviceInitialState(
                        eq(ExternalEntryPoint.DESKTOP_DEFAULT),
                        eq(
                                CrossDeviceInitialState
                                        .SIGNED_IN_WITH_DIFFERENT_ACCOUNT_TARGET_ACCOUNT_ON_DEVICE));
    }

    private void verifyBottomSheetStartSigninFlow(@Nullable CoreAccountId accountId) {
        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        String expectedTitleString;
        String expectedSubtitleString;
        if (mIsWebSignin) {
            expectedTitleString =
                    ApplicationProvider.getApplicationContext()
                            .getString(R.string.signin_account_picker_bottom_sheet_title);
            expectedSubtitleString =
                    ApplicationProvider.getApplicationContext()
                            .getString(
                                    R.string
                                            .signin_account_picker_bottom_sheet_subtitle_for_web_signin);
            ArgumentCaptor<DelegateContext> delegateContextCaptor =
                    ArgumentCaptor.forClass(DelegateContext.class);
            verify(mCoordinatorMock)
                    .startSigninFlow(configCaptor.capture(), delegateContextCaptor.capture());
            Assert.assertNotNull(delegateContextCaptor.getValue());
            SigninDelegateContext delegateContext =
                    (SigninDelegateContext) delegateContextCaptor.getValue();
            Assert.assertEquals(mContinueUrl, delegateContext.getContinueUrl());
            Assert.assertEquals(TAB_ID, delegateContext.getTabId());
        } else {
            expectedTitleString =
                    ApplicationProvider.getApplicationContext()
                            .getString(
                                    R.string
                                            .signin_account_picker_bottom_sheet_title_for_extensions,
                                    TEST_EXTENSION_NAME);
            expectedSubtitleString =
                    ApplicationProvider.getApplicationContext()
                            .getString(
                                    R.string
                                            .signin_account_picker_bottom_sheet_subtitle_for_extensions);
            verify(mCoordinatorMock).startSigninFlow(configCaptor.capture());
        }
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        Assert.assertEquals(accountId, config.selectedCoreAccountId);
        Assert.assertEquals(
                new AccountPickerBottomSheetStrings.Builder(expectedTitleString)
                        .setSubtitleString(expectedSubtitleString)
                        .setDismissButtonString(
                                ApplicationProvider.getApplicationContext()
                                        .getString(R.string.signin_account_picker_dismiss_button))
                        .build(),
                config.bottomSheetStrings);
    }

    @Test
    @SmallTest
    @EnableFeatures(SigninFeatures.OPEN_SYSTEM_ACCOUNT_SETTINGS_DIRECTLY)
    public void testOpenAccountManagementScreen_desktop_flagEnabled() {
        DeviceInfo.setIsDesktopForTesting(true);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(activity));

        SigninBridge.openAccountManagementScreen(
                mWindowAndroidMock, GAIAServiceType.GAIA_SERVICE_TYPE_NONE);

        Intent intent = shadowOf(activity).getNextStartedActivity();
        Assert.assertNotNull(intent);
        Assert.assertEquals(Settings.ACTION_SYNC_SETTINGS, intent.getAction());
        Assert.assertEquals(
                Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);
    }

    @Test
    @SmallTest
    @DisableFeatures(SigninFeatures.OPEN_SYSTEM_ACCOUNT_SETTINGS_DIRECTLY)
    public void testOpenAccountManagementScreen_desktop_flagDisabled() {
        DeviceInfo.setIsDesktopForTesting(true);

        SigninBridge.openAccountManagementScreen(
                mWindowAndroidMock, GAIAServiceType.GAIA_SERVICE_TYPE_NONE);

        Intent intent =
                shadowOf((Application) ApplicationProvider.getApplicationContext())
                        .getNextStartedActivity();
        Assert.assertNotNull(intent);
        Assert.assertNotEquals(Settings.ACTION_SYNC_SETTINGS, intent.getAction());
        // When SettingsInTab is enabled on desktop, settings opens in a browser tab via
        // ChromeLauncherActivity (chrome://settings) instead of starting SettingsActivity.
        Assert.assertEquals(
                SettingsInTab.isEnabled()
                        ? ChromeLauncherActivity.class.getName()
                        : SettingsActivity.class.getName(),
                intent.getComponent().getClassName());
    }

    @Test
    @SmallTest
    @EnableFeatures(SigninFeatures.OPEN_SYSTEM_ACCOUNT_SETTINGS_DIRECTLY)
    public void testOpenAccountManagementScreen_nonDesktop_flagEnabled() {
        DeviceInfo.setIsDesktopForTesting(false);

        SigninBridge.openAccountManagementScreen(
                mWindowAndroidMock, GAIAServiceType.GAIA_SERVICE_TYPE_NONE);

        Intent intent =
                shadowOf((Application) ApplicationProvider.getApplicationContext())
                        .getNextStartedActivity();
        Assert.assertNotNull(intent);
        Assert.assertNotEquals(Settings.ACTION_SYNC_SETTINGS, intent.getAction());
        Assert.assertEquals(SettingsActivity.class.getName(), intent.getComponent().getClassName());
    }
}
