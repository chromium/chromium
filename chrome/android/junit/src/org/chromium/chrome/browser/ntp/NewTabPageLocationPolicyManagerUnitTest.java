// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.url_constants.PolicyUrlOverrideRegistry;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Unit tests for {@link NewTabPageLocationPolicyManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NewTabPageLocationPolicyManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private PrefChangeRegistrar mPrefChangeRegistrar;
    @Captor private ArgumentCaptor<PrefChangeRegistrar.PrefObserver> mObserverCaptor;

    private NewTabPageLocationPolicyManager mManager;

    @Before
    public void setUp() {
        UserPrefs.setPrefServiceForTesting(mPrefService);
        NewTabPageLocationPolicyManager.setPrefChangeRegistrarForTesting(mPrefChangeRegistrar);

        mManager = NewTabPageLocationPolicyManager.getInstance();
        mManager.onFinishNativeInitialization(mProfile);
        verify(mPrefChangeRegistrar)
                .addObserver(eq(Pref.NEW_TAB_PAGE_LOCATION_OVERRIDE), mObserverCaptor.capture());
    }

    @After
    public void tearDown() {
        mManager.destroy();

        NewTabPageLocationPolicyManager.setPrefChangeRegistrarForTesting(null);
        PolicyUrlOverrideRegistry.resetRegistry();
    }

    @Test
    public void testOnFinishNativeInitialization_PolicySet() {
        when(mPrefService.isManagedPreference(Pref.NEW_TAB_PAGE_LOCATION_OVERRIDE))
                .thenReturn(true);

        mObserverCaptor.getValue().onPreferenceChange();

        assertTrue(PolicyUrlOverrideRegistry.getNewTabPageLocationOverrideEnabled());
    }

    @Test
    public void testOnFinishNativeInitialization_PolicyNotSet() {
        when(mPrefService.isManagedPreference(Pref.NEW_TAB_PAGE_LOCATION_OVERRIDE))
                .thenReturn(false);

        mObserverCaptor.getValue().onPreferenceChange();

        assertFalse(PolicyUrlOverrideRegistry.getNewTabPageLocationOverrideEnabled());
    }

    @Test
    public void testOnPreferenceChange_PolicySet() {
        when(mPrefService.isManagedPreference(Pref.NEW_TAB_PAGE_LOCATION_OVERRIDE))
                .thenReturn(true);

        mObserverCaptor.getValue().onPreferenceChange();

        assertTrue(PolicyUrlOverrideRegistry.getNewTabPageLocationOverrideEnabled());
    }

    @Test
    public void testOnPreferenceChange_PolicyCleared() {
        // Set a policy first.
        when(mPrefService.isManagedPreference(Pref.NEW_TAB_PAGE_LOCATION_OVERRIDE))
                .thenReturn(true);

        // Now clear it.
        when(mPrefService.isManagedPreference(Pref.NEW_TAB_PAGE_LOCATION_OVERRIDE))
                .thenReturn(false);
        mObserverCaptor.getValue().onPreferenceChange();

        assertFalse(PolicyUrlOverrideRegistry.getNewTabPageLocationOverrideEnabled());
    }

    @Test
    public void testDestroy() {
        mManager.destroy();

        verify(mPrefChangeRegistrar).removeObserver(eq(Pref.NEW_TAB_PAGE_LOCATION_OVERRIDE));
    }
}
