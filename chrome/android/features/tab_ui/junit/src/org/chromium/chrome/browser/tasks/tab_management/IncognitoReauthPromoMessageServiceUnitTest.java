// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.INCOGNITO_REAUTH_PROMO_CARD_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.INCOGNITO_REAUTH_PROMO_SHOW_COUNT;

import android.content.Context;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.Callback;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** Robolectric tests for {@link IncognitoReauthPromoMessageService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = VERSION_CODES.R)
@LooperMode(Mode.PAUSED)
public class IncognitoReauthPromoMessageServiceUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Profile mProfileMock;
    @Mock private Context mContextMock;
    @Mock private SnackbarManager mSnackbarManagerMock;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private MessageService.MessageObserver mMessageObserverMock;
    @Mock private ReauthenticatorBridge mReauthenticatorBridgeMock;
    @Captor private ArgumentCaptor<LifecycleObserver> mLifecycleObserverArgumentCaptor;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarArgumentCaptor;

    private SharedPreferencesManager mSharedPreferenceManager;
    private IncognitoReauthPromoMessageService mIncognitoReauthPromoMessageService;
    private IncognitoReauthManager mIncognitoReauthManager;
    private PauseResumeWithNativeObserver mPauseResumeWithNativeObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfileMock)).thenReturn(mPrefServiceMock);

        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(false);
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();
    }

    private void createIncognitoReauthPromoMessageService() {
        mIncognitoReauthManager = new IncognitoReauthManager(mReauthenticatorBridgeMock);
        mIncognitoReauthPromoMessageService =
                new IncognitoReauthPromoMessageService(
                        MessageType.FOR_TESTING,
                        mProfileMock,
                        mContextMock,
                        mSharedPreferenceManager,
                        mIncognitoReauthManager,
                        mSnackbarManagerMock,
                        mActivityLifecycleDispatcherMock);
        verify(mActivityLifecycleDispatcherMock, times(1))
                .register(mLifecycleObserverArgumentCaptor.capture());
        mPauseResumeWithNativeObserver =
                (PauseResumeWithNativeObserver) mLifecycleObserverArgumentCaptor.getValue();
    }

    @After
    public void tearDown() {
        mIncognitoReauthPromoMessageService.destroy();
        verifyNoMoreInteractions(mProfileMock, mContextMock, mSnackbarManagerMock);
        verify(mActivityLifecycleDispatcherMock, atLeastOnce())
                .unregister(mLifecycleObserverArgumentCaptor.getValue());
    }

    @Test
    @SmallTest
    public void testDismissMessage_SendsInvalidNotification_AndDisablesPromo() {
        createIncognitoReauthPromoMessageService();
        mIncognitoReauthPromoMessageService.addObserver(mMessageObserverMock);
        assertTrue(
                "Observer was not added.",
                mIncognitoReauthPromoMessageService
                        .getObserversForTesting()
                        .hasObserver(mMessageObserverMock));
        doNothing().when(mMessageObserverMock).messageInvalidate(MessageType.FOR_TESTING);

        mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        mIncognitoReauthPromoMessageService.dismiss();

        verify(mMessageObserverMock, times(1)).messageInvalidate(MessageType.FOR_TESTING);
        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    @SmallTest
    public void testDismissMessageWhenGTSEnabled_RecordsCorrectImpressionMetric() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.IncognitoReauth.PromoImpressionAfterActionCount", 1);

        createIncognitoReauthPromoMessageService();

        mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        mIncognitoReauthPromoMessageService.dismiss();

        // Verify the the metric is recorded.
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testPreparePromoMessage_Fails_WhenReauthIsAlreadyEnabled() {
        createIncognitoReauthPromoMessageService();
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(true);
        assertFalse(
                "Promo message shouldn't be prepared when the re-auth setting is on.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
        assertFalse(
                "Preference should also show disabled for the promo",
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, false));
    }

    @Test
    @SmallTest
    public void testPreparePromoMessage_Fails_WhenReauthFeatureNotAvailable() {
        createIncognitoReauthPromoMessageService();
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ false);

        assertFalse(
                "Promo message shouldn't be prepared when the re-auth feature is not available.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
        assertFalse(
                "Preference should also show disabled for the promo",
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, false));
    }

    @Test
    @SmallTest
    public void testPreparePromoMessage_Fails_ScreenLockNotEnabled() {
        createIncognitoReauthPromoMessageService();
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(/* value= */ false);

        assertFalse(
                "Promo message shouldn't be prepared if screen lock is not set-up.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
        assertFalse(
                "Preference should be disabled for the promo",
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, false));
    }

    @Test
    @SmallTest
    public void testPreparePromoMessage_Succeeds() {
        createIncognitoReauthPromoMessageService();
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);

        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(/* value= */ true);
        assertTrue(
                "Promo message should have been prepared.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
    }

    @Test
    @SmallTest
    public void testAddObserver_Succeeds_AndNotifiesObserverOfMessagePrepared() {
        createIncognitoReauthPromoMessageService();
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);

        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(/* value= */ true);
        doNothing().when(mMessageObserverMock).messageReady(eq(MessageType.FOR_TESTING), any());

        mIncognitoReauthPromoMessageService.addObserver(mMessageObserverMock);

        assertTrue(
                "Observer was not added.",
                mIncognitoReauthPromoMessageService
                        .getObserversForTesting()
                        .hasObserver(mMessageObserverMock));
        verify(mMessageObserverMock, times(1)).messageReady(eq(MessageType.FOR_TESTING), any());
    }

    @Test
    @SmallTest
    public void testIncreasePromoCount_IncreaseTheCountBy1() {
        createIncognitoReauthPromoMessageService();

        int currentCount = mIncognitoReauthPromoMessageService.getPromoShowCount();
        mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        int newCount = mIncognitoReauthPromoMessageService.getPromoShowCount();
        assertEquals("The count should be increased by only 1.", currentCount + 1, newCount);
    }

    @Test
    @SmallTest
    public void testIncreasePromoCount_DisablesCardIfCountExceeds() {
        createIncognitoReauthPromoMessageService();
        mSharedPreferenceManager.writeInt(
                INCOGNITO_REAUTH_PROMO_SHOW_COUNT,
                mIncognitoReauthPromoMessageService.mMaxPromoMessageCount + 1);
        mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    @SmallTest
    public void testIncreasePromoCount_DoesNotDisablesCardIfCountBelowThreshold() {
        createIncognitoReauthPromoMessageService();
        int currentCount = mIncognitoReauthPromoMessageService.getPromoShowCount();
        mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        int newCount = mIncognitoReauthPromoMessageService.getPromoShowCount();
        assertEquals("The count should be increased by 1.", currentCount + 1, newCount);
        assertTrue(mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    @SmallTest
    public void testPreparePromoMessage_Fails_AfterMaxShowCountReached() {
        createIncognitoReauthPromoMessageService();
        assert mIncognitoReauthPromoMessageService.mMaxPromoMessageCount == 10
                : "When animation is disabled, then the max count should be set to 10, as there's"
                        + " no double counting anymore.";

        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(/* value= */ true);

        // Mocking the maximum limit.
        final int initialShowCount = mIncognitoReauthPromoMessageService.getPromoShowCount();
        final int maxShowCount = mIncognitoReauthPromoMessageService.mMaxPromoMessageCount;
        for (int i = initialShowCount; i < maxShowCount; ++i) {
            assertTrue(
                    "Promo message should have been prepared as the current count: "
                            + i
                            + ", is less than the max count: "
                            + maxShowCount,
                    mIncognitoReauthPromoMessageService.preparePromoMessage());
            mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        }

        assertFalse(
                "We shouldn't prepare the message since the max limit was reached in the previous"
                        + " step.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
    }

    @Test
    @SmallTest
    public void testPreparePromoMessage_DismissesCard_WhenShowCountExceeds() {
        createIncognitoReauthPromoMessageService();
        // Exceed the max count.
        mSharedPreferenceManager.writeInt(
                INCOGNITO_REAUTH_PROMO_SHOW_COUNT,
                mIncognitoReauthPromoMessageService.mMaxPromoMessageCount + 1);
        // Ensure that promo can be shown.
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(true);

        doNothing().when(mMessageObserverMock).messageInvalidate(MessageType.FOR_TESTING);
        // This calls the prepare message internally.
        mIncognitoReauthPromoMessageService.addObserver(mMessageObserverMock);

        verify(mMessageObserverMock, times(1)).messageInvalidate(MessageType.FOR_TESTING);
        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    @SmallTest
    public void
            testReviewActionProvider_triggersIncognitoReauth_Success_EnablesPref_And_Dismisses() {
        createIncognitoReauthPromoMessageService();

        mIncognitoReauthPromoMessageService.addObserver(mMessageObserverMock);
        assertTrue(
                "Observer was not added.",
                mIncognitoReauthPromoMessageService
                        .getObserversForTesting()
                        .hasObserver(mMessageObserverMock));
        doNothing().when(mMessageObserverMock).messageInvalidate(MessageType.FOR_TESTING);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        when(mReauthenticatorBridgeMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(0);
                            callback.onResult(true);
                            return true;
                        })
                .when(mReauthenticatorBridgeMock)
                .reauthenticate(notNull());

        // Setup snackbar interaction.
        final String snackBarTestString = "This is written inside the snackbar.";
        when(mContextMock.getString(R.string.incognito_reauth_snackbar_text))
                .thenReturn(snackBarTestString);
        when(mContextMock.getColor(R.color.snackbar_background_color_baseline_dark))
                .thenReturn(R.color.snackbar_background_color_baseline_dark);
        doNothing().when(mSnackbarManagerMock).showSnackbar(mSnackbarArgumentCaptor.capture());

        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(true);
        mIncognitoReauthPromoMessageService.review();
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(false);

        verify(mReauthenticatorBridgeMock, times(1)).getBiometricAvailabilityStatus();
        verify(mReauthenticatorBridgeMock, times(1)).reauthenticate(notNull());
        verify(mPrefServiceMock, times(1))
                .setBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, true);
        verify(mMessageObserverMock, times(1)).messageInvalidate(MessageType.FOR_TESTING);

        verify(mContextMock, times(1)).getString(R.string.incognito_reauth_snackbar_text);
        verify(mContextMock, times(1)).getColor(R.color.snackbar_background_color_baseline_dark);
        verify(mSnackbarManagerMock, times(1)).showSnackbar(mSnackbarArgumentCaptor.getValue());

        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    @SmallTest
    public void testReviewActionProvider_Dismisses_IfReauthIsEnabled() {
        createIncognitoReauthPromoMessageService();
        mIncognitoReauthPromoMessageService.addObserver(mMessageObserverMock);
        assertTrue(
                "Observer was not added.",
                mIncognitoReauthPromoMessageService
                        .getObserversForTesting()
                        .hasObserver(mMessageObserverMock));
        doNothing().when(mMessageObserverMock).messageInvalidate(MessageType.FOR_TESTING);

        // Enable the Chrome Incognito lock setting.
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(true);
        mIncognitoReauthPromoMessageService.review();

        // Dismiss should be called.
        verify(mMessageObserverMock, times(1)).messageInvalidate(MessageType.FOR_TESTING);
        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    @SmallTest
    public void
            testReviewActionProvider_SendsInvalidNotification_ButDoNotDisable_IfAnyOtherIssue() {
        createIncognitoReauthPromoMessageService();
        mIncognitoReauthPromoMessageService.addObserver(mMessageObserverMock);
        assertTrue(
                "Observer was not added.",
                mIncognitoReauthPromoMessageService
                        .getObserversForTesting()
                        .hasObserver(mMessageObserverMock));
        doNothing().when(mMessageObserverMock).messageInvalidate(MessageType.FOR_TESTING);

        // Promo disabled.
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(false);
        // Ensure the reason is not because the Chrome lock setting was on.
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);
        mIncognitoReauthPromoMessageService.review();

        // Dismiss should be called.
        verify(mMessageObserverMock, times(1)).messageInvalidate(MessageType.FOR_TESTING);
        // The promo card should be still enabled in this case.
        assertTrue(mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    @SmallTest
    public void testResumeAction_DismissesPromo_IfReauthIsEnabled() {
        createIncognitoReauthPromoMessageService();

        mIncognitoReauthPromoMessageService.addObserver(mMessageObserverMock);
        assertTrue(
                "Observer was not added.",
                mIncognitoReauthPromoMessageService
                        .getObserversForTesting()
                        .hasObserver(mMessageObserverMock));
        doNothing().when(mMessageObserverMock).messageInvalidate(MessageType.FOR_TESTING);
        // Promo disabled.
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(false);
        // Turn on the Chrome lock setting is on.
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(true);

        // Emulate `onResume`
        mPauseResumeWithNativeObserver.onResumeWithNative();

        // Dismiss should be called.
        verify(mMessageObserverMock, times(1)).messageInvalidate(MessageType.FOR_TESTING);
        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    @SmallTest
    public void testResumeAction_SendsInvalidNotification_ButDoNotDisable_IfAnyOtherIssue() {
        createIncognitoReauthPromoMessageService();

        mIncognitoReauthPromoMessageService.addObserver(mMessageObserverMock);
        assertTrue(
                "Observer was not added.",
                mIncognitoReauthPromoMessageService
                        .getObserversForTesting()
                        .hasObserver(mMessageObserverMock));
        doNothing().when(mMessageObserverMock).messageInvalidate(MessageType.FOR_TESTING);
        // Disable promo
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(false);
        // Ensure the reason is not because the Chrome lock setting was on.
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);

        // Emulate `onResume`
        mPauseResumeWithNativeObserver.onResumeWithNative();

        // Dismiss should be called.
        verify(mMessageObserverMock, times(1)).messageInvalidate(MessageType.FOR_TESTING);
        assertTrue(mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    @SmallTest
    public void testResumeAction_FiresMessageReady_AfterEnablingPromoAgain_ForOnResume() {
        createIncognitoReauthPromoMessageService();

        mIncognitoReauthPromoMessageService.addObserver(mMessageObserverMock);
        assertTrue(
                "Observer was not added.",
                mIncognitoReauthPromoMessageService
                        .getObserversForTesting()
                        .hasObserver(mMessageObserverMock));
        doNothing().when(mMessageObserverMock).messageInvalidate(MessageType.FOR_TESTING);
        doNothing().when(mMessageObserverMock).messageReady(eq(MessageType.FOR_TESTING), any());

        // Disable promo
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(false);
        // Ensure the reason is not because the Chrome lock setting was on.
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);

        mPauseResumeWithNativeObserver.onResumeWithNative();
        // Dismiss should be called.
        verify(mMessageObserverMock, times(1)).messageInvalidate(MessageType.FOR_TESTING);
        assertTrue(mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));

        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(true);
        mPauseResumeWithNativeObserver.onResumeWithNative();

        verify(mMessageObserverMock, times(1)).messageReady(eq(MessageType.FOR_TESTING), any());
        assertTrue(mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }
}
