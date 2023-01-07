// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import org.junit.Assert;
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
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridgeJni;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;

/**
 * JUnit tests of the class {@link StepDisplayHandlerImpl}.
 * This test suite can be significantly compressed if @ParameterizedTest from JUnit5 can be used.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class StepDisplayHandlerImplTest {
    @Rule
    public JniMocker mocker = new JniMocker();
    @Rule
    public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock
    private SafeBrowsingBridge.Natives mSBNativesMock;

    private StepDisplayHandler mStepDisplayHandler;

    @Before
    public void setUp() {
        mocker.mock(SafeBrowsingBridgeJni.TEST_HOOKS, mSBNativesMock);
        mStepDisplayHandler = new StepDisplayHandlerImpl();
    }

    private void setSBState(@SafeBrowsingState int sbState) {
        Mockito.when(mSBNativesMock.getSafeBrowsingState()).thenReturn(sbState);
    }

    @Test
    public void testDisplaySBStepWhenSBEnhanced() {
        setSBState(SafeBrowsingState.ENHANCED_PROTECTION);
        Assert.assertTrue(mStepDisplayHandler.shouldDisplaySafeBrowsing());
    }

    @Test
    public void testDisplaySBWhenSBStandard() {
        setSBState(SafeBrowsingState.STANDARD_PROTECTION);
        Assert.assertTrue(mStepDisplayHandler.shouldDisplaySafeBrowsing());
    }

    @Test
    public void testDontDisplaySBWhenSBUnsafe() {
        setSBState(SafeBrowsingState.NO_SAFE_BROWSING);
        Assert.assertFalse(mStepDisplayHandler.shouldDisplaySafeBrowsing());
    }
}
