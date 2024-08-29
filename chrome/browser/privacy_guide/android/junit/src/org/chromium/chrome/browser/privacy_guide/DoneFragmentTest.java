// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

/** Tests for {@link DoneFragment} */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_PRIVACY_GUIDE_AD_TOPICS})
public class DoneFragmentTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mMocker = new JniMocker();

    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private PrivacySandboxBridge.Natives mPrivacySandboxBridge;

    private FragmentScenario mScenario;
    private DoneFragment mFragment;
    private View mPsButton;
    private View mWaaButton;
    private String mPrivacySandboxDescriptionText;

    private void initFragment() {
        mScenario =
                FragmentScenario.launchInContainer(
                        DoneFragment.class,
                        Bundle.EMPTY,
                        R.style.Theme_MaterialComponents,
                        new FragmentFactory() {
                            @NonNull
                            @Override
                            public Fragment instantiate(
                                    @NonNull ClassLoader classLoader, @NonNull String className) {
                                Fragment fragment = super.instantiate(classLoader, className);
                                if (fragment instanceof DoneFragment) {
                                    ((DoneFragment) fragment).setProfile(mProfile);
                                }
                                return fragment;
                            }
                        });
        mScenario.onFragment(
                fragment -> {
                    mFragment = (DoneFragment) fragment;
                    mPsButton = fragment.getView().findViewById(R.id.ps_button);
                    mWaaButton = fragment.getView().findViewById(R.id.waa_button);
                    mPrivacySandboxDescriptionText =
                            ((TextView) fragment.getView().findViewById(R.id.ps_description))
                                    .getText()
                                    .toString();
                });
    }

    private void setSignedInState(boolean isSignedIn) {
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(isSignedIn);
    }

    private void setPrivacySandboxState(boolean isRestricted, boolean isRestrictedNoticeEnabled) {
        when(mPrivacySandboxBridge.isPrivacySandboxRestricted(mProfile)).thenReturn(isRestricted);
        when(mPrivacySandboxBridge.isRestrictedNoticeEnabled(mProfile))
                .thenReturn(isRestrictedNoticeEnabled);
    }

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        mMocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mPrivacySandboxBridge);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
    }

    @Test
    public void testPSButtonNotVisible() {
        setPrivacySandboxState(true, false);
        initFragment();

        assertFalse(mPsButton.isShown());
    }

    @Test
    public void testPSButtonVisibleWhenNotRestricted() {
        setPrivacySandboxState(false, false);
        initFragment();

        assertTrue(mPsButton.isShown());
    }

    @Test
    public void testPSButtonVisibleWhenRestrictedNoticeEnabled() {
        setPrivacySandboxState(true, true);
        initFragment();

        assertTrue(mPsButton.isShown());
    }

    @Test
    public void testPSButtonVisibleWhenNotRestrictedAndRestrictedNoticeEnabled() {
        setPrivacySandboxState(false, true);
        initFragment();

        assertTrue(mPsButton.isShown());
    }

    @Test
    public void testWaaButtonVisibleWhenSignedIn() {
        setSignedInState(true);
        initFragment();

        assertTrue(mWaaButton.isShown());
    }

    @Test
    public void testWaaButtonNotVisibleWhenNotSignedIn() {
        setSignedInState(false);
        initFragment();

        assertFalse(mWaaButton.isShown());
    }

    @Test
    public void testPrivacySandboxDescriptionForAdTopicsIsDisplayedWhenAdTopicsIsEnabled() {
        setPrivacySandboxState(false, false);
        initFragment();

        String privacySandboxDescriptionAdTopicsString =
                ApplicationProvider.getApplicationContext()
                        .getResources()
                        .getString(R.string.privacy_guide_privacy_sandbox_description_ad_topics);
        assertEquals(mPrivacySandboxDescriptionText, privacySandboxDescriptionAdTopicsString);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_PRIVACY_GUIDE_AD_TOPICS})
    public void testPrivacySandboxDescriptionIsDisplayedWhenAdTopicsIsDisabled() {
        setPrivacySandboxState(false, false);
        initFragment();

        String privacySandboxDescriptionString =
                ApplicationProvider.getApplicationContext()
                        .getResources()
                        .getString(R.string.privacy_guide_privacy_sandbox_description);
        assertEquals(mPrivacySandboxDescriptionText, privacySandboxDescriptionString);
    }
}
