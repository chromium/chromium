// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link AuxiliarySearchBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION})
public final class AuxiliarySearchBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private AuxiliarySearchBridge.Natives mMockAuxiliarySearchBridgeJni;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        AuxiliarySearchBridgeJni.setInstanceForTesting(mMockAuxiliarySearchBridgeJni);
    }

    @After
    public void tearDown() {}

    @Test
    @SmallTest
    public void getForProfileTest() {
        doReturn(false).when(mProfile).isOffTheRecord();
        AuxiliarySearchBridge bridge = new AuxiliarySearchBridge(mProfile);
        assertNotNull(bridge);

        verify(mMockAuxiliarySearchBridgeJni).getForProfile(mProfile);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ANDROID_APP_INTEGRATION)
    @EnableFeatures(ChromeFeatureList.ANDROID_APP_INTEGRATION_V2)
    public void getForProfileTestV2() {
        doReturn(false).when(mProfile).isOffTheRecord();
        AuxiliarySearchBridge bridge = new AuxiliarySearchBridge(mProfile);
        assertNotNull(bridge);

        verify(mMockAuxiliarySearchBridgeJni).getForProfile(mProfile);
    }
}
