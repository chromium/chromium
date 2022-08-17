// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;

import androidx.appcompat.widget.SwitchCompat;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridgeJni;

/**
 * JUnit tests of the class {@link MSBBViewHolder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class PrivacyGuideMSBBViewHolderTest {
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private Profile mProfile;
    @Mock
    private UnifiedConsentServiceBridge.Natives mNativeMock;

    private Activity mActivity;
    private View mMSBBView;
    private SwitchCompat mMSBBButton;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Profile.setLastUsedProfileForTesting(mProfile);
        mocker.mock(UnifiedConsentServiceBridgeJni.TEST_HOOKS, mNativeMock);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mMSBBView = mActivity.getLayoutInflater().inflate(R.layout.privacy_guide_msbb_step, null);
        mActivity.setContentView(mMSBBView);
        mMSBBButton = mMSBBView.findViewById(R.id.msbb_switch);
    }

    @Test
    @SmallTest
    public void testSwichOffWhenMSBBOff() {
        Mockito.when(mNativeMock.isUrlKeyedAnonymizedDataCollectionEnabled(mProfile))
                .thenReturn(false);
        MSBBViewHolder msbbViewHolder = new MSBBViewHolder(mMSBBView);
        assertFalse(mMSBBButton.isChecked());
    }

    @Test
    @SmallTest
    public void testSwitchOnWhenMSBBOn() {
        Mockito.when(mNativeMock.isUrlKeyedAnonymizedDataCollectionEnabled(mProfile))
                .thenReturn(true);
        MSBBViewHolder msbbViewHolder = new MSBBViewHolder(mMSBBView);
        assertTrue(mMSBBButton.isChecked());
    }

    @Test
    @SmallTest
    public void testTurnMSBBOn() {
        Mockito.when(mNativeMock.isUrlKeyedAnonymizedDataCollectionEnabled(mProfile))
                .thenReturn(false);
        MSBBViewHolder msbbViewHolder = new MSBBViewHolder(mMSBBView);
        mMSBBButton.performClick();
        Mockito.verify(mNativeMock).setUrlKeyedAnonymizedDataCollectionEnabled(mProfile, true);
    }

    @Test
    @SmallTest
    public void testTurnMSBBOff() {
        Mockito.when(mNativeMock.isUrlKeyedAnonymizedDataCollectionEnabled(mProfile))
                .thenReturn(true);
        MSBBViewHolder msbbViewHolder = new MSBBViewHolder(mMSBBView);
        mMSBBButton.performClick();
        Mockito.verify(mNativeMock).setUrlKeyedAnonymizedDataCollectionEnabled(mProfile, false);
    }
}
