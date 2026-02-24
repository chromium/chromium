// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.INCOGNITO_REAUTH_PROMO_CARD_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.INCOGNITO_REAUTH_PROMO_SHOW_COUNT;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE;

import android.content.Context;
import android.content.res.Resources;
import android.os.Build.VERSION_CODES;

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
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
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
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** Robolectric tests for {@link IncognitoReauthPromoMessageService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = VERSION_CODES.R)
public class IncognitoReauthPromoMessageServiceUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private PrefService mPrefService;

    @Mock private ServiceDismissActionProvider<@MessageType Integer> mServiceDismissActionProvider;

    @Mock private ReauthenticatorBridge mReauthenticatorBridge;
    @Captor private ArgumentCaptor<LifecycleObserver> mLifecycleObserverArgumentCaptor;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarArgumentCaptor;

    private SharedPreferencesManager mSharedPreferenceManager;
    private IncognitoReauthPromoMessageService mIncognitoReauthPromoMessageService;
    private IncognitoReauthManager mIncognitoReauthManager;
    private PauseResumeWithNativeObserver mPauseResumeWithNativeObserver;

    @Before
    public void setUp() {
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);

        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(false);
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();

        when(mContext.getResources()).thenReturn(mResources);
    }

    private void createIncognitoReauthPromoMessageService() {
        mIncognitoReauthManager = new IncognitoReauthManager(mReauthenticatorBridge);
        mIncognitoReauthPromoMessageService =
                new IncognitoReauthPromoMessageService(
                        mProfile,
                        mContext,
                        mSharedPreferenceManager,
                        mIncognitoReauthManager,
                        mSnackbarManager,
                        mActivityLifecycleDispatcher);
        verify(mActivityLifecycleDispatcher).register(mLifecycleObserverArgumentCaptor.capture());
        mPauseResumeWithNativeObserver =
                (PauseResumeWithNativeObserver) mLifecycleObserverArgumentCaptor.getValue();
    }

    @After
    public void tearDown() {
        mIncognitoReauthPromoMessageService.destroy();
        verifyNoMoreInteractions(mProfile, mContext, mSnackbarManager);
        verify(mActivityLifecycleDispatcher, atLeastOnce())
                .unregister(mLifecycleObserverArgumentCaptor.getValue());
    }

    @Test
    public void testDismissMessage_SendsInvalidNotification_AndDisablesPromo() {
        createIncognitoReauthPromoMessageService();
        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);

        mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        mIncognitoReauthPromoMessageService.dismiss();

        verify(mServiceDismissActionProvider).dismiss(INCOGNITO_REAUTH_PROMO_MESSAGE);
        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    public void testDismissMessageWhenGTSEnabled_RecordsCorrectImpressionMetric() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.IncognitoReauth.PromoImpressionAfterActionCount", 1);

        createIncognitoReauthPromoMessageService();
        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);

        mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        mIncognitoReauthPromoMessageService.dismiss();

        // Verify the the metric is recorded.
        histogramWatcher.assertExpected();
    }

    @Test
    public void testPreparePromoMessage_Fails_WhenReauthIsAlreadyEnabled() {
        createIncognitoReauthPromoMessageService();
        when(mPrefService.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID)).thenReturn(true);
        assertFalse(
                "Promo message shouldn't be prepared when the re-auth setting is on.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
        assertFalse(
                "Preference should also show disabled for the promo",
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, false));
    }

    @Test
    public void testPreparePromoMessage_Fails_WhenReauthFeatureNotAvailable() {
        createIncognitoReauthPromoMessageService();
        when(mPrefService.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
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
    public void testPreparePromoMessage_Fails_ScreenLockNotEnabled() {
        createIncognitoReauthPromoMessageService();
        when(mPrefService.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
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
    public void testPreparePromoMessage_Succeeds() {
        createIncognitoReauthPromoMessageService();
        when(mPrefService.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);

        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(/* value= */ true);
        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);

        assertTrue(
                "Promo message should have been prepared.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
        verify(mContext, atLeastOnce()).getResources();
        verify(mContext, atLeastOnce()).getString(anyInt());
    }

    @Test
    public void testInitialize_Succeeds_AndQueuesMessage() {
        createIncognitoReauthPromoMessageService();
        when(mPrefService.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);

        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(/* value= */ true);

        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);

        assertEquals(1, mIncognitoReauthPromoMessageService.getMessageItems().size());
        verify(mContext, atLeastOnce()).getResources();
        verify(mContext, atLeastOnce()).getString(anyInt());
    }

    @Test
    public void testIncreasePromoCount_IncreaseTheCountBy1() {
        createIncognitoReauthPromoMessageService();

        int currentCount = mIncognitoReauthPromoMessageService.getPromoShowCount();
        mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        int newCount = mIncognitoReauthPromoMessageService.getPromoShowCount();
        assertEquals("The count should be increased by only 1.", currentCount + 1, newCount);
    }

    @Test
    public void testIncreasePromoCount_DisablesCardIfCountExceeds() {
        createIncognitoReauthPromoMessageService();
        mSharedPreferenceManager.writeInt(
                INCOGNITO_REAUTH_PROMO_SHOW_COUNT,
                mIncognitoReauthPromoMessageService.mMaxPromoMessageCount + 1);
        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);

        mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    public void testIncreasePromoCount_DoesNotDisablesCardIfCountBelowThreshold() {
        createIncognitoReauthPromoMessageService();
        int currentCount = mIncognitoReauthPromoMessageService.getPromoShowCount();
        mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        int newCount = mIncognitoReauthPromoMessageService.getPromoShowCount();
        assertEquals("The count should be increased by 1.", currentCount + 1, newCount);
        assertTrue(mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    public void testPreparePromoMessage_Fails_AfterMaxShowCountReached() {
        createIncognitoReauthPromoMessageService();
        assertWithMessage(
                        "When animation is disabled, then the max count should be set to 10, as"
                                + " there's no double counting anymore.")
                .that(mIncognitoReauthPromoMessageService.mMaxPromoMessageCount)
                .isEqualTo(10);

        when(mPrefService.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(/* value= */ true);

        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);

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

        verify(mContext, atLeastOnce()).getResources();
        verify(mContext, atLeastOnce()).getString(anyInt());
    }

    @Test
    public void testPreparePromoMessage_DismissesCard_WhenShowCountExceeds() {
        createIncognitoReauthPromoMessageService();
        // Exceed the max count.
        mSharedPreferenceManager.writeInt(
                INCOGNITO_REAUTH_PROMO_SHOW_COUNT,
                mIncognitoReauthPromoMessageService.mMaxPromoMessageCount + 1);
        // Ensure that promo can be shown.
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(true);

        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);

        verify(mServiceDismissActionProvider).dismiss(INCOGNITO_REAUTH_PROMO_MESSAGE);
        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    public void
            testReviewActionProvider_triggersIncognitoReauth_Success_EnablesPref_And_Dismisses() {
        createIncognitoReauthPromoMessageService();

        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /* isAvailable= */ true);
        when(mReauthenticatorBridge.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(0);
                            callback.onResult(true);
                            return true;
                        })
                .when(mReauthenticatorBridge)
                .reauthenticate(notNull());

        // Setup snackbar interaction.
        final String snackBarTestString = "This is written inside the snackbar.";
        when(mContext.getString(R.string.incognito_reauth_snackbar_text))
                .thenReturn(snackBarTestString);
        when(mContext.getColor(R.color.floating_snackbar_background_incognito))
                .thenReturn(R.color.floating_snackbar_background_incognito);
        doNothing().when(mSnackbarManager).showSnackbar(mSnackbarArgumentCaptor.capture());

        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(true);
        mIncognitoReauthPromoMessageService.review();
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(false);

        verify(mReauthenticatorBridge).getBiometricAvailabilityStatus();
        verify(mReauthenticatorBridge).reauthenticate(notNull());
        verify(mPrefService).setBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, true);
        verify(mServiceDismissActionProvider).dismiss(INCOGNITO_REAUTH_PROMO_MESSAGE);

        verify(mContext).getString(R.string.incognito_reauth_snackbar_text);
        verify(mContext).getColor(R.color.floating_snackbar_background_incognito);
        verify(mSnackbarManager).showSnackbar(mSnackbarArgumentCaptor.getValue());

        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    public void testReviewActionProvider_Dismisses_IfReauthIsEnabled() {
        createIncognitoReauthPromoMessageService();
        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);

        // Enable the Chrome Incognito lock setting.
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);
        when(mPrefService.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID)).thenReturn(true);
        mIncognitoReauthPromoMessageService.review();

        // Dismiss should be called.
        verify(mServiceDismissActionProvider).dismiss(INCOGNITO_REAUTH_PROMO_MESSAGE);
        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    public void
            testReviewActionProvider_SendsInvalidNotification_ButDoNotDisable_IfAnyOtherIssue() {
        createIncognitoReauthPromoMessageService();
        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);

        // Promo disabled.
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(false);
        // Ensure the reason is not because the Chrome lock setting was on.
        when(mPrefService.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);
        mIncognitoReauthPromoMessageService.review();

        // Dismiss should be called.
        verify(mServiceDismissActionProvider).dismiss(INCOGNITO_REAUTH_PROMO_MESSAGE);
        // The promo card should be still enabled in this case.
        assertTrue(mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    public void testResumeAction_DismissesPromo_IfReauthIsEnabled() {
        createIncognitoReauthPromoMessageService();

        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);
        // Promo disabled.
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(false);
        // Turn on the Chrome lock setting is on.
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(true);
        when(mPrefService.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID)).thenReturn(true);

        // Emulate `onResume`
        mPauseResumeWithNativeObserver.onResumeWithNative();

        // Dismiss should be called.
        verify(mServiceDismissActionProvider).dismiss(INCOGNITO_REAUTH_PROMO_MESSAGE);
        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    public void testResumeAction_SendsInvalidNotification_ButDoNotDisable_IfAnyOtherIssue() {
        createIncognitoReauthPromoMessageService();

        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);
        // Disable promo
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(false);
        // Ensure the reason is not because the Chrome lock setting was on.
        when(mPrefService.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);

        // Emulate `onResume`
        mPauseResumeWithNativeObserver.onResumeWithNative();

        // Dismiss should be called.
        verify(mServiceDismissActionProvider).dismiss(INCOGNITO_REAUTH_PROMO_MESSAGE);
        assertTrue(mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    public void testResumeAction_FiresMessageReady_AfterEnablingPromoAgain_ForOnResume() {
        createIncognitoReauthPromoMessageService();

        mIncognitoReauthPromoMessageService.initialize(mServiceDismissActionProvider);

        // Disable promo
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(false);
        // Ensure the reason is not because the Chrome lock setting was on.
        when(mPrefService.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);

        mPauseResumeWithNativeObserver.onResumeWithNative();
        // Dismiss should be called.
        verify(mServiceDismissActionProvider).dismiss(INCOGNITO_REAUTH_PROMO_MESSAGE);
        assertTrue(mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));

        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(true);
        mPauseResumeWithNativeObserver.onResumeWithNative();

        assertEquals(1, mIncognitoReauthPromoMessageService.getMessageItems().size());
        assertTrue(mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
        verify(mContext, atLeastOnce()).getResources();
        verify(mContext, atLeastOnce()).getString(anyInt());
    }
}
