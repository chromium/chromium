// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.safety_check.SafetyCheckProperties.COMPROMISED_PASSWORDS;
import static org.chromium.chrome.browser.safety_check.SafetyCheckProperties.PASSWORDS_STATE;
import static org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SAFE_BROWSING_STATE;
import static org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UPDATES_STATE;

import android.os.Handler;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckUIStatus;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.safety_check.SafetyCheckMediator.SafetyCheckInteractions;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.PasswordsState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.chrome.browser.signin.ui.SigninActivityLauncher;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Unit tests for {@link SafetyCheckMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class SafetyCheckMediatorTest {
    private static final String SAFETY_CHECK_INTERACTIONS_HISTOGRAM =
            "Settings.SafetyCheck.Interactions";
    private static final String SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM =
            "Settings.SafetyCheck.PasswordsResult";
    private static final String SAFETY_CHECK_SAFE_BROWSING_RESULT_HISTOGRAM =
            "Settings.SafetyCheck.SafeBrowsingResult";
    private static final String SAFETY_CHECK_UPDATES_RESULT_HISTOGRAM =
            "Settings.SafetyCheck.UpdatesResult";

    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();

    private PropertyModel mModel;
    @Mock
    private SafetyCheckUpdatesDelegate mUpdatesDelegate;
    @Mock
    private SigninActivityLauncher mSigninLauncher;
    @Mock
    private SettingsLauncher mSettingsLauncher;
    @Mock
    private SafetyCheckBridge mBridge;
    @Mock
    private Handler mHandler;
    @Mock
    private PasswordCheck mPasswordCheck;

    private SafetyCheckMediator mMediator;

    private void passwordDiskDataAvailable() {
        doAnswer(invocation -> {
            PasswordCheck.Observer observer =
                    (PasswordCheck.Observer) (invocation.getArguments()[0]);
            observer.onCompromisedCredentialsFetchCompleted();
            observer.onSavedPasswordsFetchCompleted();
            return null;
        })
                .when(mPasswordCheck)
                .addObserver(mMediator, true);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = SafetyCheckProperties.createSafetyCheckModel();
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
        mMediator = new SafetyCheckMediator(
                mModel, mUpdatesDelegate, mSettingsLauncher, mSigninLauncher, mBridge, mHandler);
        // Execute any delayed tasks immediately.
        doAnswer(invocation -> {
            Runnable runnable = (Runnable) (invocation.getArguments()[0]);
            runnable.run();
            return null;
        })
                .when(mHandler)
                .postDelayed(any(Runnable.class), anyLong());
        // User is always signed in unless the test specifies otherwise.
        when(mBridge.userSignedIn()).thenReturn(true);
        // Reset the histogram count.
        ShadowRecordHistogram.reset();
    }

    @Test
    public void testStartInteractionRecorded() {
        mMediator.performSafetyCheck();
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_INTERACTIONS_HISTOGRAM, SafetyCheckInteractions.STARTED));
    }

    @Test
    public void testUpdatesCheckUpdated() {
        doAnswer(invocation -> {
            Callback<Integer> callback =
                    ((WeakReference<Callback<Integer>>) invocation.getArguments()[0]).get();
            callback.onResult(UpdatesState.UPDATED);
            return null;
        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));

        mMediator.performSafetyCheck();
        assertEquals(UpdatesState.UPDATED, mModel.get(UPDATES_STATE));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_UPDATES_RESULT_HISTOGRAM, UpdateStatus.UPDATED));
    }

    @Test
    public void testUpdatesCheckOutdated() {
        doAnswer(invocation -> {
            Callback<Integer> callback =
                    ((WeakReference<Callback<Integer>>) invocation.getArguments()[0]).get();
            callback.onResult(UpdatesState.OUTDATED);
            return null;
        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));

        mMediator.performSafetyCheck();
        assertEquals(UpdatesState.OUTDATED, mModel.get(UPDATES_STATE));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_UPDATES_RESULT_HISTOGRAM, UpdateStatus.OUTDATED));
    }

    @Test
    public void testSafeBrowsingCheckEnabledStandard() {
        doAnswer(invocation -> {
            mMediator.onSafeBrowsingCheckResult(SafeBrowsingStatus.ENABLED_STANDARD);
            return null;
        })
                .when(mBridge)
                .checkSafeBrowsing();

        mMediator.performSafetyCheck();
        assertEquals(SafeBrowsingState.ENABLED_STANDARD, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_SAFE_BROWSING_RESULT_HISTOGRAM,
                        SafeBrowsingStatus.ENABLED_STANDARD));
    }

    @Test
    public void testSafeBrowsingCheckDisabled() {
        doAnswer(invocation -> {
            mMediator.onSafeBrowsingCheckResult(SafeBrowsingStatus.DISABLED);
            return null;
        })
                .when(mBridge)
                .checkSafeBrowsing();

        mMediator.performSafetyCheck();
        assertEquals(SafeBrowsingState.DISABLED, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_SAFE_BROWSING_RESULT_HISTOGRAM, SafeBrowsingStatus.DISABLED));
    }

    @Test
    public void testPasswordsCheckError() {
        doAnswer(invocation -> {
            mMediator.onPasswordCheckStatusChanged(PasswordCheckUIStatus.ERROR_UNKNOWN);
            return null;
        })
                .when(mPasswordCheck)
                .startCheck();

        mMediator.performSafetyCheck();
        mMediator.onCompromisedCredentialsFetchCompleted();
        mMediator.onSavedPasswordsFetchCompleted();
        assertEquals(PasswordsState.ERROR, mModel.get(PASSWORDS_STATE));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.ERROR));
    }

    @Test
    public void testPasswordsCheckNoPasswords() {
        doAnswer(invocation -> {
            mMediator.onPasswordCheckStatusChanged(PasswordCheckUIStatus.ERROR_NO_PASSWORDS);
            return null;
        })
                .when(mPasswordCheck)
                .startCheck();
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(0);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(0);

        mMediator.performSafetyCheck();
        mMediator.onCompromisedCredentialsFetchCompleted();
        mMediator.onSavedPasswordsFetchCompleted();
        assertEquals(PasswordsState.NO_PASSWORDS, mModel.get(PASSWORDS_STATE));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.NO_PASSWORDS));
    }

    @Test
    public void testPasswordsCheckNoLeaks() {
        doAnswer(invocation -> {
            mMediator.onPasswordCheckStatusChanged(PasswordCheckUIStatus.IDLE);
            return null;
        })
                .when(mPasswordCheck)
                .startCheck();
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(15);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(0);

        mMediator.performSafetyCheck();
        mMediator.onCompromisedCredentialsFetchCompleted();
        mMediator.onSavedPasswordsFetchCompleted();
        assertEquals(PasswordsState.SAFE, mModel.get(PASSWORDS_STATE));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.SAFE));
    }

    @Test
    public void testPasswordsCheckHasLeaks() {
        int numLeaks = 123;
        doAnswer(invocation -> {
            mMediator.onPasswordCheckStatusChanged(PasswordCheckUIStatus.IDLE);
            return null;
        })
                .when(mPasswordCheck)
                .startCheck();
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(199);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(numLeaks);

        mMediator.performSafetyCheck();
        mMediator.onCompromisedCredentialsFetchCompleted();
        mMediator.onSavedPasswordsFetchCompleted();
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
        assertEquals(numLeaks, mModel.get(COMPROMISED_PASSWORDS));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM,
                        PasswordsStatus.COMPROMISED_EXIST));
    }

    @Test
    public void testNullStateLessThan10MinsPasswordsSafeState() {
        // Ran just now.
        SharedPreferencesManager preferenceManager = SharedPreferencesManager.getInstance();
        preferenceManager.writeLong(ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                System.currentTimeMillis());
        // Safe Browsing: on.
        doAnswer(invocation -> {
            mMediator.onSafeBrowsingCheckResult(SafeBrowsingStatus.ENABLED_STANDARD);
            return null;
        })
                .when(mBridge)
                .checkSafeBrowsing();
        // Passwords: safe state.
        passwordDiskDataAvailable();
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(12);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(0);
        // Updates: outdated.
        doAnswer(invocation -> {
            Callback<Integer> callback =
                    ((WeakReference<Callback<Integer>>) invocation.getArguments()[0]).get();
            callback.onResult(UpdatesState.OUTDATED);
            return null;
        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));
        mMediator.setInitialState();
        // Verify the states.
        assertEquals(SafeBrowsingState.ENABLED_STANDARD, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.SAFE, mModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.OUTDATED, mModel.get(UPDATES_STATE));
    }

    @Test
    public void testNullStateLessThan10MinsNoSavedPasswords() {
        // Ran just now.
        SharedPreferencesManager preferenceManager = SharedPreferencesManager.getInstance();
        preferenceManager.writeLong(ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                System.currentTimeMillis());
        // Safe Browsing: disabled by admin.
        doAnswer(invocation -> {
            mMediator.onSafeBrowsingCheckResult(SafeBrowsingStatus.DISABLED_BY_ADMIN);
            return null;
        })
                .when(mBridge)
                .checkSafeBrowsing();
        // Passwords: no passwords.
        passwordDiskDataAvailable();
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(0);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(0);
        // Updates: offline.
        doAnswer(invocation -> {
            Callback<Integer> callback =
                    ((WeakReference<Callback<Integer>>) invocation.getArguments()[0]).get();
            callback.onResult(UpdatesState.OFFLINE);
            return null;
        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));
        mMediator.setInitialState();
        // Verify the states.
        assertEquals(SafeBrowsingState.DISABLED_BY_ADMIN, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.NO_PASSWORDS, mModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.OFFLINE, mModel.get(UPDATES_STATE));
    }

    @Test
    public void testNullStateLessThan10MinsPasswordsUnsafeState() {
        // Ran just now.
        SharedPreferencesManager preferenceManager = SharedPreferencesManager.getInstance();
        preferenceManager.writeLong(ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                System.currentTimeMillis());
        // Safe Browsing: off.
        doAnswer(invocation -> {
            mMediator.onSafeBrowsingCheckResult(SafeBrowsingStatus.DISABLED);
            return null;
        })
                .when(mBridge)
                .checkSafeBrowsing();
        // Passwords: compromised state.
        passwordDiskDataAvailable();
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(20);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(18);
        // Updates: updated.
        doAnswer(invocation -> {
            Callback<Integer> callback =
                    ((WeakReference<Callback<Integer>>) invocation.getArguments()[0]).get();
            callback.onResult(UpdatesState.UPDATED);
            return null;
        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));
        mMediator.setInitialState();
        // Verify the states.
        assertEquals(SafeBrowsingState.DISABLED, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.UPDATED, mModel.get(UPDATES_STATE));
    }

    @Test
    public void testNullStateMoreThan10MinsPasswordsSafeState() {
        // Ran 20 mins ago.
        SharedPreferencesManager preferenceManager = SharedPreferencesManager.getInstance();
        preferenceManager.writeLong(ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                System.currentTimeMillis() - (20 * 60 * 1000));
        // Safe Browsing: on.
        doAnswer(invocation -> {
            mMediator.onSafeBrowsingCheckResult(SafeBrowsingStatus.ENABLED_STANDARD);
            return null;
        })
                .when(mBridge)
                .checkSafeBrowsing();
        // Passwords: safe state.
        passwordDiskDataAvailable();
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(13);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(0);
        // Updates: outdated.
        doAnswer(invocation -> {
            Callback<Integer> callback =
                    ((WeakReference<Callback<Integer>>) invocation.getArguments()[0]).get();
            callback.onResult(UpdatesState.OUTDATED);
            return null;
        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));
        mMediator.setInitialState();
        // Verify the states.
        assertEquals(SafeBrowsingState.UNCHECKED, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.UNCHECKED, mModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.UNCHECKED, mModel.get(UPDATES_STATE));
    }

    @Test
    public void testNullStateMoreThan10MinsPasswordsUnsafeState() {
        // Ran 20 mins ago.
        SharedPreferencesManager preferenceManager = SharedPreferencesManager.getInstance();
        preferenceManager.writeLong(ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                System.currentTimeMillis() - (20 * 60 * 1000));
        // Safe Browsing: off.
        doAnswer(invocation -> {
            mMediator.onSafeBrowsingCheckResult(SafeBrowsingStatus.DISABLED);
            return null;
        })
                .when(mBridge)
                .checkSafeBrowsing();
        // Passwords: compromised state.
        passwordDiskDataAvailable();
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(20);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(18);
        // Updates: updated.
        doAnswer(invocation -> {
            Callback<Integer> callback =
                    ((WeakReference<Callback<Integer>>) invocation.getArguments()[0]).get();
            callback.onResult(UpdatesState.UPDATED);
            return null;
        })
                .when(mUpdatesDelegate)
                .checkForUpdates(any(WeakReference.class));
        mMediator.setInitialState();
        // Verify the states.
        assertEquals(SafeBrowsingState.UNCHECKED, mModel.get(SAFE_BROWSING_STATE));
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
        assertEquals(UpdatesState.UNCHECKED, mModel.get(UPDATES_STATE));
    }

    @Test
    public void testPasswordsInitialLoadDuringInitialState() {
        // Order: initial state -> load completed -> done.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        mMediator.onCompromisedCredentialsFetchCompleted();
        // Not complete fetch - still checking.
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        // Data available.
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(20);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(18);
        mMediator.onSavedPasswordsFetchCompleted();
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
    }

    @Test
    public void testPasswordsInitialLoadDuringRunningCheck() {
        // Order: initial state -> safety check triggered -> load completed -> check done.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(20);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(18);
        mMediator.onSavedPasswordsFetchCompleted();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        mMediator.onCompromisedCredentialsFetchCompleted();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        mMediator.onPasswordCheckStatusChanged(PasswordCheckUIStatus.IDLE);
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM,
                        PasswordsStatus.COMPROMISED_EXIST));
    }

    @Test
    public void testPasswordsInitialLoadAfterRunningCheck() {
        // Order: initial state -> safety check triggered -> check done -> load completed.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        mMediator.onPasswordCheckStatusChanged(PasswordCheckUIStatus.IDLE);
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(20);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(18);
        mMediator.onSavedPasswordsFetchCompleted();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        mMediator.onCompromisedCredentialsFetchCompleted();
        assertEquals(PasswordsState.COMPROMISED_EXIST, mModel.get(PASSWORDS_STATE));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM,
                        PasswordsStatus.COMPROMISED_EXIST));
    }

    @Test
    public void testPasswordsInitialLoadCheckReturnsError() {
        // Order: initial state -> safety check triggered -> check error -> load ignored.
        mMediator.setInitialState();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        mMediator.performSafetyCheck();
        assertEquals(PasswordsState.CHECKING, mModel.get(PASSWORDS_STATE));

        mMediator.onPasswordCheckStatusChanged(PasswordCheckUIStatus.ERROR_UNKNOWN);
        assertEquals(PasswordsState.ERROR, mModel.get(PASSWORDS_STATE));

        // Previous check found compromises.
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(20);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(18);
        // The results of the previous check should be ignored.
        mMediator.onSavedPasswordsFetchCompleted();
        assertEquals(PasswordsState.ERROR, mModel.get(PASSWORDS_STATE));

        mMediator.onCompromisedCredentialsFetchCompleted();
        assertEquals(PasswordsState.ERROR, mModel.get(PASSWORDS_STATE));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.ERROR));
    }

    @Test
    public void testPasswordsInitialLoadUserSignedOut() {
        // Order: initial state is user signed out -> load ignored.
        when(mBridge.userSignedIn()).thenReturn(false);
        mMediator.setInitialState();
        assertEquals(PasswordsState.SIGNED_OUT, mModel.get(PASSWORDS_STATE));

        // Previous check found compromises.
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(20);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(18);
        // The results of the previous check should be ignored.
        mMediator.onSavedPasswordsFetchCompleted();
        assertEquals(PasswordsState.SIGNED_OUT, mModel.get(PASSWORDS_STATE));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SAFETY_CHECK_PASSWORDS_RESULT_HISTOGRAM, PasswordsStatus.SIGNED_OUT));
    }
}
