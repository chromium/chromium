// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;

/** Tests for the Safety Hub Magic Stack bridge. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MagicStackBridgeTest {
    private static final String DESCRIPTION = "description";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock MagicStackBridge.Natives mNatives;
    @Mock MagicStackBridge.Observer mObserver;
    @Mock Profile mProfile;

    private MagicStackBridge mBridge;

    @Before
    public void setUp() {
        mJniMocker.mock(MagicStackBridgeJni.TEST_HOOKS, mNatives);
        mBridge = MagicStackBridge.getForProfile(mProfile);
        mBridge.addObserver(mObserver);
    }

    @Test
    public void testGetModuleToShow() {
        MagicStackEntry expected =
                MagicStackEntry.create(DESCRIPTION, MagicStackEntry.ModuleType.SAFE_BROWSING);
        doReturn(expected).when(mNatives).getModuleToShow(mProfile);
        MagicStackEntry observed = mBridge.getModuleToShow();
        assertEquals(expected, observed);
    }

    @Test
    public void testDismissActiveModule() {
        mBridge.dismissActiveModule();
        verify(mNatives).dismissActiveModule(mProfile);
        verify(mObserver).activeModuleDismissed();
    }

    @Test
    public void testDismissSafeBrowsingModule() {
        mBridge.dismissSafeBrowsingModule();
        verify(mNatives).dismissSafeBrowsingModule(mProfile);
        verify(mObserver, times(0)).activeModuleDismissed();
    }

    @Test
    public void testDismissCompromisedPasswordsModule() {
        mBridge.dismissCompromisedPasswordsModule();
        verify(mNatives).dismissCompromisedPasswordsModule(mProfile);
        verify(mObserver, times(0)).activeModuleDismissed();
    }
}
