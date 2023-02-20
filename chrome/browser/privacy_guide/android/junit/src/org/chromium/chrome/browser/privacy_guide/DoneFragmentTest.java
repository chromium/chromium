// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.View;

import androidx.fragment.app.testing.FragmentScenario;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Tests for {@link DoneFragment}
 */
@RunWith(BaseRobolectricTestRunner.class)
public class DoneFragmentTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Profile mProfile;
    @Mock
    private IdentityServicesProvider mIdentityServicesProvider;
    @Mock
    private IdentityManager mIdentityManager;

    private FragmentScenario mScenario;
    private DoneFragment mFragment;
    private View mPsButton;
    private View mWaaButton;

    private void initFragmentWithSignInState(boolean isSignedIn) {
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(isSignedIn);
        mScenario = FragmentScenario.launchInContainer(
                DoneFragment.class, Bundle.EMPTY, R.style.Theme_MaterialComponents);
        mScenario.onFragment(fragment -> {
            mFragment = (DoneFragment) fragment;
            mPsButton = fragment.getView().findViewById(R.id.ps_button);
            mWaaButton = fragment.getView().findViewById(R.id.waa_button);
        });
    }

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mProfile);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
        IdentityServicesProvider.setInstanceForTests(null);
        Profile.setLastUsedProfileForTesting(null);
    }

    @Test
    public void testOneLinkVisibleWhenSignedOut() {
        initFragmentWithSignInState(false);
        assertTrue(mPsButton.isShown());
        assertFalse(mWaaButton.isShown());
    }

    @Test
    public void testTwoLinksVisibleWhenSignedIn() {
        initFragmentWithSignInState(true);
        assertTrue(mPsButton.isShown());
        assertTrue(mWaaButton.isShown());
    }
}
