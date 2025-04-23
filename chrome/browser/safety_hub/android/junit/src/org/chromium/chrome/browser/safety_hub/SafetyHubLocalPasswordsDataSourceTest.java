// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.safety_hub.SafetyHubLocalPasswordsDataSource.ModuleType;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.base.TestActivity;

/** Robolectric tests for {@link SafetyHubLocalPasswordsDataSource}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Features.EnableFeatures({
    ChromeFeatureList.SAFETY_HUB,
    ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
    ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
})
public class SafetyHubLocalPasswordsDataSourceTest {
    private static class SafetyHubLocalPasswordsDataSourceObserverTest
            implements SafetyHubLocalPasswordsDataSource.Observer {
        @ModuleType int mModuleType;

        @Override
        public void localPasswordsStateChanged(@ModuleType int moduleType) {
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private SafetyHubExpandablePreference mPreference;
    private SafetyHubLocalPasswordsDataSource mDataSource;
    private SafetyHubLocalPasswordsDataSourceObserverTest mObserver;

    @Mock private SafetyHubModuleDelegate mModuleDelegateMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private SafetyHubFetchService mSafetyHubFetchServiceMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private PasswordStoreBridge mPasswordStoreBridge;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        mPreference = new SafetyHubExpandablePreference(mActivity, null);

        mObserver = new SafetyHubLocalPasswordsDataSourceObserverTest();
        mDataSource =
                new SafetyHubLocalPasswordsDataSource(
                        mModuleDelegateMock,
                        mPrefServiceMock,
                        mSafetyHubFetchServiceMock,
                        mPasswordStoreBridge);
        mDataSource.addObserver(mObserver);
        mDataSource.setUp();
    }

    public void mockTotalPasswordsCount(int totalPasswordsCount) {
        doReturn(totalPasswordsCount).when(mModuleDelegateMock).getLocalPasswordsCount(any());
    }

    private void mockPasswordCounts(int compromised, int weak, int reused) {
        doReturn(compromised)
                .when(mPrefServiceMock)
                .getInteger(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT);
        doReturn(weak).when(mPrefServiceMock).getInteger(Pref.LOCAL_WEAK_CREDENTIALS_COUNT);
        doReturn(reused).when(mPrefServiceMock).getInteger(Pref.LOCAL_REUSED_CREDENTIALS_COUNT);
    }

    private void mockRunPasswordCheckup(boolean willRun) {
        doReturn(willRun).when(mSafetyHubFetchServiceMock).runLocalPasswordCheckup();
    }

    @Test
    public void countsUnavailable_lastCheckLongAgo() {
        mockTotalPasswordsCount(1);
        mockPasswordCounts(/* compromised= */ -1, /* weak= */ -1, /* reused= */ -1);
        mockRunPasswordCheckup(true);

        assertTrue(mDataSource.maybeTriggerPasswordCheckup());
        verify(mSafetyHubFetchServiceMock, times(1)).runLocalPasswordCheckup();

        mDataSource.localPasswordCountsChanged();
        mDataSource.onSavedPasswordsChanged(0);
        mDataSource.updateState();
        assertEquals(ModuleType.UNAVAILABLE_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    public void countsUnavailable_lastCheckRecently() {
        mockTotalPasswordsCount(1);
        mockPasswordCounts(/* compromised= */ -1, /* weak= */ -1, /* reused= */ -1);
        mockRunPasswordCheckup(false);

        assertFalse(mDataSource.maybeTriggerPasswordCheckup());
        verify(mSafetyHubFetchServiceMock, times(1)).runLocalPasswordCheckup();

        mDataSource.localPasswordCountsChanged();
        mDataSource.onSavedPasswordsChanged(0);
        mDataSource.updateState();
        assertEquals(ModuleType.UNAVAILABLE_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    public void noPasswords() {
        mockTotalPasswordsCount(0);

        mDataSource.localPasswordCountsChanged();
        mDataSource.onSavedPasswordsChanged(0);
        mDataSource.updateState();

        assertEquals(ModuleType.NO_SAVED_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    public void hasCompromisedPasswords() {
        mockTotalPasswordsCount(5);
        mockPasswordCounts(/* compromised= */ 4, 0, 0);

        mDataSource.localPasswordCountsChanged();
        mDataSource.onSavedPasswordsChanged(0);
        mDataSource.updateState();

        assertEquals(ModuleType.HAS_COMPROMISED_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    public void noCompromisedPasswords_hasWeakAndReusedPasswords_enabled() {
        mockTotalPasswordsCount(5);
        mockPasswordCounts(/* compromised= */ 0, /* weak= */ 2, /* reused= */ 1);

        mDataSource.localPasswordCountsChanged();
        mDataSource.onSavedPasswordsChanged(0);
        mDataSource.updateState();

        assertEquals(ModuleType.HAS_REUSED_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)
    public void noCompromisedPasswords_hasWeakAndReusedPasswords_disabled() {
        mockTotalPasswordsCount(5);
        mockPasswordCounts(/* compromised= */ 0, /* weak= */ 2, /* reused= */ 1);

        mDataSource.localPasswordCountsChanged();
        mDataSource.onSavedPasswordsChanged(0);
        mDataSource.updateState();

        assertEquals(ModuleType.NO_COMPROMISED_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    public void noCompromisedPasswords_hasWeakPasswords_enabled() {
        mockTotalPasswordsCount(5);
        mockPasswordCounts(/* compromised= */ 0, /* weak= */ 1, /* reused= */ 0);

        mDataSource.localPasswordCountsChanged();
        mDataSource.onSavedPasswordsChanged(0);
        mDataSource.updateState();

        assertEquals(ModuleType.HAS_WEAK_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)
    public void noCompromisedPasswords_hasWeakPasswords_disabled() {
        mockTotalPasswordsCount(5);
        mockPasswordCounts(/* compromised= */ 0, /* weak= */ 1, /* reused= */ 0);

        mDataSource.localPasswordCountsChanged();
        mDataSource.onSavedPasswordsChanged(0);
        mDataSource.updateState();

        assertEquals(ModuleType.NO_COMPROMISED_PASSWORDS, mObserver.getModuleType());
    }

    @Test
    public void noCompromisedPasswords() {
        mockTotalPasswordsCount(5);
        mockPasswordCounts(0, 0, 0);

        mDataSource.localPasswordCountsChanged();
        mDataSource.onSavedPasswordsChanged(0);
        mDataSource.updateState();

        assertEquals(ModuleType.NO_COMPROMISED_PASSWORDS, mObserver.getModuleType());
    }
}
