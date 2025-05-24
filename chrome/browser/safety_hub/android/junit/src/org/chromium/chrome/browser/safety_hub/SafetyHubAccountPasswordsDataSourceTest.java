// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.annotation.DrawableRes;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safety_hub.SafetyHubAccountPasswordsDataSource.ModuleType;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GaiaId;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.TestActivity;

/** Robolectric tests for {@link SafetyHubAccountPasswordsDataSource}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubAccountPasswordsDataSourceTest {
    private static class SafetyHubAccountPasswordsDataSourceObserverTest
            implements SafetyHubAccountPasswordsDataSource.Observer {
        @ModuleType int mModuleType;

        @Override
        public void accountPasswordsStateChanged(@ModuleType int moduleType) {
            mModuleType = moduleType;
        }

        public @ModuleType int getModuleType() {
            return mModuleType;
        }
    }

    private static final @DrawableRes int SAFE_ICON = R.drawable.material_ic_check_24dp;
    private static final @DrawableRes int INFO_ICON = R.drawable.btn_info;
    private static final @DrawableRes int MANAGED_ICON = R.drawable.ic_business;
    private static final @DrawableRes int WARNING_ICON = R.drawable.ic_error;

    private static final String TEST_EMAIL_ADDRESS = "test@email.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private SafetyHubExpandablePreference mPreference;
    private SafetyHubAccountPasswordsDataSource mDataSource;
    private SafetyHubAccountPasswordsDataSourceObserverTest mObserver;

    @Mock private SafetyHubModuleDelegate mModuleDelegateMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private SafetyHubFetchService mSafetyHubFetchServiceMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock private IdentityManager mIdentityManager;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        mPreference = new SafetyHubExpandablePreference(mActivity, null);

        doReturn(mIdentityManager).when(mIdentityServicesProviderMock).getIdentityManager(mProfile);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        mockPasswordCounts(0, 0, 0);
        mockTotalPasswordsCount(0);
        mockSignedInState(false);

        mObserver = new SafetyHubAccountPasswordsDataSourceObserverTest();
        mDataSource =
                new SafetyHubAccountPasswordsDataSource(
                        mModuleDelegateMock,
                        mPrefServiceMock,
                        mSafetyHubFetchServiceMock,
                        mSigninManagerMock,
                        mProfile);
        mDataSource.addObserver(mObserver);
        mDataSource.setUp();
    }

    public void mockSignedInState(boolean isSignedIn) {
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(isSignedIn);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(
                        isSignedIn
                                ? CoreAccountInfo.createFromEmailAndGaiaId(
                                        TEST_EMAIL_ADDRESS, new GaiaId("0"))
                                : null);
        if (!isSignedIn) {
            doReturn(-1).when(mPrefServiceMock).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        }
    }

    public void mockTotalPasswordsCount(int totalPasswordsCount) {
        doReturn(totalPasswordsCount).when(mModuleDelegateMock).getAccountPasswordsCount(any());
    }

    private void mockPasswordCounts(int compromised, int weak, int reused) {
        doReturn(compromised).when(mPrefServiceMock).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        doReturn(weak).when(mPrefServiceMock).getInteger(Pref.WEAK_CREDENTIALS_COUNT);
        doReturn(reused).when(mPrefServiceMock).getInteger(Pref.REUSED_CREDENTIALS_COUNT);
    }

    @Test
    public void noCompromisedPasswords() {
        int totalPasswordsCount = 5;
        mockPasswordCounts(0, 0, 0);
        mockSignedInState(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mDataSource.updateState();

        assertEquals(ModuleType.NO_COMPROMISED_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void noCompromisedPasswords_weakAndReusedPasswordsExists_enabled() {
        int weakPasswordsCount = 1;
        int reusedPasswordsCount = 2;
        int totalPasswordsCount = 5;
        mockPasswordCounts(/* compromised= */ 0, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mDataSource.updateState();

        assertEquals(ModuleType.HAS_REUSED_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void noCompromisedPasswords_weakPasswordsExists_enabled() {
        int weakPasswordsCount = 1;
        int totalPasswordsCount = 5;
        mockPasswordCounts(/* compromised= */ 0, weakPasswordsCount, /* reused= */ 0);
        mockSignedInState(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mDataSource.updateState();

        assertEquals(ModuleType.HAS_WEAK_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)
    public void noCompromisedPasswords_weakAndReusedPasswordsExists_disabled() {
        int totalPasswordsCount = 5;
        int weakPasswordsCount = 1;
        int reusedPasswordsCount = 2;
        mockPasswordCounts(/* compromised= */ 0, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mDataSource.updateState();

        assertEquals(ModuleType.NO_COMPROMISED_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void compromisedPasswordsExist() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = 5;
        int weakPasswordsCount = 6;
        int reusedPasswordsCount = 6;
        mockPasswordCounts(compromisedPasswordsCount, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mDataSource.updateState();

        assertEquals(ModuleType.HAS_COMPROMISED_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    public void noPasswordsSaved() {
        mockPasswordCounts(0, 0, 0);
        mockSignedInState(true);
        mockTotalPasswordsCount(0);

        mDataSource.updateState();

        assertEquals(ModuleType.NO_SAVED_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void compromisedCountUnavailable_noWeakAndReusedPasswords_disabled() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = -1;
        int weakPasswordsCount = 0;
        int reusedPasswordsCount = 0;
        mockPasswordCounts(compromisedPasswordsCount, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mDataSource.updateState();

        assertEquals(ModuleType.UNAVAILABLE_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void compromisedCountUnavailable_noWeakAndReusedPasswords_enabled() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = -1;
        int weakPasswordsCount = 0;
        int reusedPasswordsCount = 0;
        mockPasswordCounts(compromisedPasswordsCount, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mDataSource.updateState();

        assertEquals(
                ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS,
                mObserver.getModuleType());
    }

    @Test
    public void signedOut() {
        mockPasswordCounts(0, 0, 0);
        mockSignedInState(false);
        mockTotalPasswordsCount(0);

        mDataSource.updateState();

        assertEquals(ModuleType.SIGNED_OUT, mObserver.getModuleType());
    }
}
