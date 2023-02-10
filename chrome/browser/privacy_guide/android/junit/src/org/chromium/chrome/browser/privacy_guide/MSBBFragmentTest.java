// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import androidx.appcompat.widget.SwitchCompat;
import androidx.fragment.app.testing.FragmentScenario;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridgeJni;

/**
 * JUnit tests of the class {@link MSBBFragment}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class MSBBFragmentTest {
    @Rule
    public JniMocker mocker = new JniMocker();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Profile mProfile;
    @Mock
    private UnifiedConsentServiceBridge.Natives mNativeMock;

    private FragmentScenario mScenario;
    private SwitchCompat mMSBBButton;
    private final UserActionTester mActionTester = new UserActionTester();

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mProfile);
        mocker.mock(UnifiedConsentServiceBridgeJni.TEST_HOOKS, mNativeMock);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
        Profile.setLastUsedProfileForTesting(null);
        mActionTester.tearDown();
    }

    private void initFragmentWithMSBBState(boolean isMSBBOn) {
        Mockito.when(mNativeMock.isUrlKeyedAnonymizedDataCollectionEnabled(mProfile))
                .thenReturn(isMSBBOn);
        mScenario = FragmentScenario.launchInContainer(
                MSBBFragment.class, Bundle.EMPTY, R.style.Theme_MaterialComponents);
        mScenario.onFragment(
                fragment -> mMSBBButton = fragment.getView().findViewById(R.id.msbb_switch));
    }

    @Test
    public void testIsSwitchOffWhenMSBBOff() {
        initFragmentWithMSBBState(false);
        assertFalse(mMSBBButton.isChecked());
    }

    @Test
    public void testIsSwitchOnWhenMSBBOn() {
        initFragmentWithMSBBState(true);
        assertTrue(mMSBBButton.isChecked());
    }

    @Test
    public void testTurnMSBBOn() {
        initFragmentWithMSBBState(false);
        mMSBBButton.performClick();
        Mockito.verify(mNativeMock).setUrlKeyedAnonymizedDataCollectionEnabled(mProfile, true);
    }

    @Test
    public void testTurnMSBBOff() {
        initFragmentWithMSBBState(true);
        mMSBBButton.performClick();
        Mockito.verify(mNativeMock).setUrlKeyedAnonymizedDataCollectionEnabled(mProfile, false);
    }

    @Test
    public void testTurnMSBBOff_changeMSBBOffUserAction() {
        initFragmentWithMSBBState(true);
        mMSBBButton.performClick();
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeMSBBOff"));
    }

    @Test
    public void testTurnMSBBOn_changeMSBBOnUserAction() {
        initFragmentWithMSBBState(false);
        mMSBBButton.performClick();
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeMSBBOn"));
    }
}
