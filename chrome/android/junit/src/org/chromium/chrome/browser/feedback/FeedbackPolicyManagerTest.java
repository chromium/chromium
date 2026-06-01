// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Tests for the {@link FeedbackPolicyManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedbackPolicyManagerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PrefService mMockPrefService;
    @Mock private PrefChangeRegistrar mMockRegistrar;
    @Mock private Profile mRegularMockProfile;
    @Captor private ArgumentCaptor<PrefChangeRegistrar.PrefObserver> mObserverCaptor;

    private SharedPreferencesManager mSharedPreferenceManager;

    @Before
    public void setup() {
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();
        UserPrefs.setPrefServiceForTesting(mMockPrefService);
        FeedbackPolicyManager.setPrefChangeRegistrarForTesting(mMockRegistrar);

        FeedbackPolicyManager.setInstanceForTesting(new FeedbackPolicyManager());
    }

    @After
    public void tearDown() {
        mSharedPreferenceManager.removeKey(ChromePreferenceKeys.POLICY_USER_FEEDBACK_ALLOWED);

        UserPrefs.setPrefServiceForTesting(null);
        FeatureOverrides.removeAllIncludingAnnotations();
    }

    private void setFeatureFlag(boolean enabled) {
        if (enabled) {
            FeatureOverrides.enable(ChromeFeatureList.USER_FEEDBACK_ALLOWED_POLICY);
        } else {
            FeatureOverrides.disable(ChromeFeatureList.USER_FEEDBACK_ALLOWED_POLICY);
        }
    }

    @Test
    public void testFlagDisabled_ReturnsDefaultTrue() {
        setFeatureFlag(false);

        Assert.assertTrue(
                "Should return true when flag is disabled",
                FeedbackPolicyManager.getInstance().isUserFeedbackAllowed());
    }

    @Test
    public void testFlagEnabled_PreNative_ReturnsDefaultTrue() {
        setFeatureFlag(true);

        Assert.assertTrue(
                "Should return true pre-native by default when flag is enabled",
                FeedbackPolicyManager.getInstance().isUserFeedbackAllowed());
    }

    @Test
    public void testFlagEnabled_PreNative_ReturnsCachedValue() {
        setFeatureFlag(true);
        mSharedPreferenceManager.writeBoolean(
                ChromePreferenceKeys.POLICY_USER_FEEDBACK_ALLOWED, false);

        Assert.assertFalse(
                "Should return cached value pre-native when flag is enabled",
                FeedbackPolicyManager.getInstance().isUserFeedbackAllowed());
    }

    @Test
    public void testFlagEnabled_NativeInit_SyncsCache() {
        setFeatureFlag(true);
        Mockito.when(mMockPrefService.getBoolean(Pref.USER_FEEDBACK_ALLOWED)).thenReturn(false);

        // Trigger native initialization
        FeedbackPolicyManager.getInstance().onFinishNativeInitialization(mRegularMockProfile);

        Assert.assertFalse(
                "Should return live native value",
                FeedbackPolicyManager.getInstance().isUserFeedbackAllowed());

        // Verify cache is updated
        Assert.assertFalse(
                "SharedPreferences should be updated to false",
                mSharedPreferenceManager.readBoolean(
                        ChromePreferenceKeys.POLICY_USER_FEEDBACK_ALLOWED, true));
    }

    @Test
    public void testNativePrefChange_UpdatesCache() {
        setFeatureFlag(true);
        Mockito.when(mMockPrefService.getBoolean(Pref.USER_FEEDBACK_ALLOWED)).thenReturn(true);

        // Initialize manager
        FeedbackPolicyManager.getInstance().onFinishNativeInitialization(mRegularMockProfile);

        // Capture the callback passed to PrefChangeRegistrar
        Mockito.verify(mMockRegistrar)
                .addObserver(Mockito.eq(Pref.USER_FEEDBACK_ALLOWED), mObserverCaptor.capture());
        PrefChangeRegistrar.PrefObserver callback = mObserverCaptor.getValue();

        // Simulate policy change to false
        Mockito.when(mMockPrefService.getBoolean(Pref.USER_FEEDBACK_ALLOWED)).thenReturn(false);
        callback.onPreferenceChange();

        // Verify cache is updated
        Assert.assertFalse(
                "SharedPreferences should be updated to false after change",
                mSharedPreferenceManager.readBoolean(
                        ChromePreferenceKeys.POLICY_USER_FEEDBACK_ALLOWED, true));
    }

    @Test
    public void testProfileSwitching_RecreatesRegistrar() {
        setFeatureFlag(true);
        Profile secondMockProfile = Mockito.mock(Profile.class);
        PrefChangeRegistrar secondMockRegistrar = Mockito.mock(PrefChangeRegistrar.class);

        // Initialize with first profile
        Mockito.when(mMockPrefService.getBoolean(Pref.USER_FEEDBACK_ALLOWED)).thenReturn(true);
        FeedbackPolicyManager.getInstance().onFinishNativeInitialization(mRegularMockProfile);

        // Switch to second profile
        FeedbackPolicyManager.setPrefChangeRegistrarForTesting(secondMockRegistrar);
        FeedbackPolicyManager.getInstance().onFinishNativeInitialization(secondMockProfile);

        // Verify that the old registrar was destroyed
        Mockito.verify(mMockRegistrar).destroy();

        // Verify that the new registrar was used to add observer
        Mockito.verify(secondMockRegistrar)
                .addObserver(Mockito.eq(Pref.USER_FEEDBACK_ALLOWED), Mockito.any());
    }

    @Test
    public void testIncognitoProfile_ObserverNotRegistered() {
        setFeatureFlag(true);
        Profile incognitoProfile = Mockito.mock(Profile.class);
        Mockito.when(incognitoProfile.isOffTheRecord()).thenReturn(true);

        FeedbackPolicyManager.getInstance().onFinishNativeInitialization(incognitoProfile);

        // Verify that the registrar was NOT used to add observer
        Mockito.verify(mMockRegistrar, Mockito.never())
                .addObserver(Mockito.eq(Pref.USER_FEEDBACK_ALLOWED), Mockito.any());
    }

    @Test
    public void testDestroy_CleansUp() {
        setFeatureFlag(true);
        Mockito.when(mMockPrefService.getBoolean(Pref.USER_FEEDBACK_ALLOWED)).thenReturn(true);

        FeedbackPolicyManager.getInstance().onFinishNativeInitialization(mRegularMockProfile);
        FeedbackPolicyManager.getInstance().destroy();

        Mockito.verify(mMockRegistrar).destroy();
    }
}
