// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
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
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/**
 * Robolectric tests for {@link IncognitoReauthPromoMessageService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = VERSION_CODES.R)
@LooperMode(Mode.PAUSED)
public class IncognitoReauthPromoMessageServiceUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private Profile mProfileMock;
    @Mock
    private Context mContextMock;
    @Mock
    private SnackbarManager mSnackbarManagerMock;
    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;
    @Mock
    private PrefService mPrefServiceMock;
    @Mock
    private MessageService.MessageObserver mMessageObserverMock;

    private SharedPreferencesManager mSharedPreferenceManager;
    private IncognitoReauthPromoMessageService mIncognitoReauthPromoMessageService;
    private boolean mIsTabToGTSAnimationEnabled;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfileMock)).thenReturn(mPrefServiceMock);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(false);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(false);
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(null);
        mSharedPreferenceManager = SharedPreferencesManager.getInstance();
    }

    private void createIncognitoReauthPromoMessageService() {
        mIncognitoReauthPromoMessageService = new IncognitoReauthPromoMessageService(
                MessageType.FOR_TESTING, mProfileMock, mContextMock, mSharedPreferenceManager,
                mSnackbarManagerMock, () -> mIsTabToGTSAnimationEnabled);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(mProfileMock, mContextMock, mSnackbarManagerMock);
    }

    @Test
    @SmallTest
    public void testDismissMessage_SendsInvalidNotification_AndDisablesPromo() {
        createIncognitoReauthPromoMessageService();
        mIncognitoReauthPromoMessageService.addObserver(mMessageObserverMock);
        assertTrue("Observer was not added.",
                mIncognitoReauthPromoMessageService.getObserversForTesting().hasObserver(
                        mMessageObserverMock));
        doNothing().when(mMessageObserverMock).messageInvalidate(MessageType.FOR_TESTING);

        mIncognitoReauthPromoMessageService.dismiss();

        verify(mMessageObserverMock, times(1)).messageInvalidate(MessageType.FOR_TESTING);
        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }

    @Test
    @SmallTest
    public void testPreparePromoMessage_Fails_WhenReauthIsAlreadyEnabled() {
        createIncognitoReauthPromoMessageService();
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(true);
        assertFalse("Promo message shouldn't be prepared when the re-auth setting is on.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
        assertFalse("Preference should also show disabled for the promo",
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, false));
    }

    @Test
    @SmallTest
    public void testPreparePromoMessage_Fails_WhenReauthFeatureNotAvailable() {
        createIncognitoReauthPromoMessageService();
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /*isAvailable=*/false);

        assertFalse(
                "Promo message shouldn't be prepared when the re-auth feature is not available.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
        assertFalse("Preference should also show disabled for the promo",
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, false));
    }

    @Test
    @SmallTest
    public void testPreparePromoMessage_Fails_ScreenLockNotEnabled() {
        createIncognitoReauthPromoMessageService();
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(/*isAvailable=*/true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(/*value=*/false);

        assertFalse("Promo message shouldn't be prepared if screen lock is not set-up.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
        assertFalse("Preference should be disabled for the promo",
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, false));
    }

    @Test
    @SmallTest
    public void testPreparePromoMessage_Succeeds() {
        createIncognitoReauthPromoMessageService();
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);

        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /*isAvailable=*/true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(/*value=*/true);
        assertTrue("Promo message should have been prepared.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
    }

    @Test
    @SmallTest
    public void testAddObserver_Succeeds_AndNotifiesObserverOfMessagePrepared() {
        createIncognitoReauthPromoMessageService();
        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);

        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /*isAvailable=*/true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(/*value=*/true);
        doNothing().when(mMessageObserverMock).messageReady(eq(MessageType.FOR_TESTING), any());

        mIncognitoReauthPromoMessageService.addObserver(mMessageObserverMock);

        assertTrue("Observer was not added.",
                mIncognitoReauthPromoMessageService.getObserversForTesting().hasObserver(
                        mMessageObserverMock));
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
        mSharedPreferenceManager.writeInt(INCOGNITO_REAUTH_PROMO_SHOW_COUNT,
                mIncognitoReauthPromoMessageService.mMaximumPromoShowCountLimit + 1);
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
    public void testPreparePromoMessage_Fails_AfterMaxShowCountReached_TabToGTSEnabled() {
        mIsTabToGTSAnimationEnabled = true;
        createIncognitoReauthPromoMessageService();
        assert mIncognitoReauthPromoMessageService.mMaximumPromoShowCountLimit
                == 20
            : "When animation is enabled, then the max count should be set to 20, because of double counting.";

        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /*isAvailable=*/true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(/*value=*/true);

        // Mocking the maximum limit.
        final int initialShowCount = mIncognitoReauthPromoMessageService.getPromoShowCount();
        final int maxShowCount = mIncognitoReauthPromoMessageService.mMaximumPromoShowCountLimit;
        for (int i = initialShowCount; i < maxShowCount; ++i) {
            assertTrue("Promo message should have been prepared as the current count: " + i
                            + ", is less than the max count: " + maxShowCount,
                    mIncognitoReauthPromoMessageService.preparePromoMessage());
            mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        }
        assertFalse(
                "We shouldn't prepare the message since the max limit was reached in the previous step.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
    }

    @Test
    @SmallTest
    public void testPreparePromoMessage_Fails_AfterMaxShowCountReached_TabToGTSDisabled() {
        mIsTabToGTSAnimationEnabled = false;
        createIncognitoReauthPromoMessageService();
        assert mIncognitoReauthPromoMessageService.mMaximumPromoShowCountLimit
                == 10
            : "When animation is disabled, then the max count should be set to 10, as there's no"
              + " double counting anymore.";

        when(mPrefServiceMock.getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID))
                .thenReturn(false);
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(
                /*isAvailable=*/true);
        IncognitoReauthSettingUtils.setIsDeviceScreenLockEnabledForTesting(/*value=*/true);

        // Mocking the maximum limit.
        final int initialShowCount = mIncognitoReauthPromoMessageService.getPromoShowCount();
        final int maxShowCount = mIncognitoReauthPromoMessageService.mMaximumPromoShowCountLimit;
        for (int i = initialShowCount; i < maxShowCount; ++i) {
            assertTrue("Promo message should have been prepared as the current count: " + i
                            + ", is less than the max count: " + maxShowCount,
                    mIncognitoReauthPromoMessageService.preparePromoMessage());
            mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        }

        assertFalse(
                "We shouldn't prepare the message since the max limit was reached in the previous step.",
                mIncognitoReauthPromoMessageService.preparePromoMessage());
    }

    @Test
    @SmallTest
    public void testPreparePromoMessage_DismissesCard_WhenShowCountExceeds() {
        createIncognitoReauthPromoMessageService();
        // Exceed the max count.
        mSharedPreferenceManager.writeInt(INCOGNITO_REAUTH_PROMO_SHOW_COUNT,
                mIncognitoReauthPromoMessageService.mMaximumPromoShowCountLimit + 1);
        // Ensure that promo can be shown.
        IncognitoReauthPromoMessageService.setIsPromoEnabledForTesting(true);

        doNothing().when(mMessageObserverMock).messageInvalidate(MessageType.FOR_TESTING);
        // This calls the prepare message internally.
        mIncognitoReauthPromoMessageService.addObserver(mMessageObserverMock);

        verify(mMessageObserverMock, times(1)).messageInvalidate(MessageType.FOR_TESTING);
        assertFalse(
                mSharedPreferenceManager.readBoolean(INCOGNITO_REAUTH_PROMO_CARD_ENABLED, true));
    }
}
