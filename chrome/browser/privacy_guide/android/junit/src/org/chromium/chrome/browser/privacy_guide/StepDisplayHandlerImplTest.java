// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

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
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridgeJni;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.sync.SyncService;

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
    @Mock
    private SyncService mSyncService;

    private StepDisplayHandler mStepDisplayHandler;

    @Before
    public void setUp() {
        SyncService.overrideForTests(mSyncService);
        mocker.mock(SafeBrowsingBridgeJni.TEST_HOOKS, mSBNativesMock);
        mStepDisplayHandler = new StepDisplayHandlerImpl();
    }

    @After
    public void tearDown() {
        SyncService.resetForTests();
    }

    private void setSBState(@SafeBrowsingState int sbState) {
        Mockito.when(mSBNativesMock.getSafeBrowsingState()).thenReturn(sbState);
    }

    private void setSyncState(boolean enabled) {
        Mockito.when(mSyncService.isSyncFeatureEnabled()).thenReturn(enabled);
    }

    @Test
    public void testDisplaySBStepWhenSBEnhanced() {
        setSBState(SafeBrowsingState.ENHANCED_PROTECTION);
        assertTrue(mStepDisplayHandler.shouldDisplaySafeBrowsing());
    }

    @Test
    public void testDisplaySBWhenSBStandard() {
        setSBState(SafeBrowsingState.STANDARD_PROTECTION);
        assertTrue(mStepDisplayHandler.shouldDisplaySafeBrowsing());
    }

    @Test
    public void testDontDisplaySBWhenSBUnsafe() {
        setSBState(SafeBrowsingState.NO_SAFE_BROWSING);
        assertFalse(mStepDisplayHandler.shouldDisplaySafeBrowsing());
    }

    @Test
    public void testDisplayHistorySyncWhenSyncOn() {
        setSyncState(true);
        assertTrue(mStepDisplayHandler.shouldDisplaySync());
    }

    @Test
    public void testDontDisplayHistorySyncWhenSyncOff() {
        setSyncState(false);
        assertFalse(mStepDisplayHandler.shouldDisplaySync());
    }
}
