// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.navigation_predictor;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link NavigationPredictorBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NavigationPredictorBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NavigationPredictorBridge.Natives mNatives;
    @Mock private ActivityLifecycleDispatcher mFirstWindowLifecycleDispatcher;
    @Mock private ActivityLifecycleDispatcher mSecondWindowLifecycleDispatcher;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        NavigationPredictorBridgeJni.setInstanceForTesting(mNatives);
    }

    @Test
    public void testPausesOnlyWhenLastResumedWindowPauses() {
        NavigationPredictorBridge firstWindow =
                new NavigationPredictorBridge(
                        mProfile, mFirstWindowLifecycleDispatcher, () -> true);
        NavigationPredictorBridge secondWindow =
                new NavigationPredictorBridge(
                        mProfile, mSecondWindowLifecycleDispatcher, () -> true);
        firstWindow.onResumeWithNative();
        secondWindow.onResumeWithNative();

        firstWindow.onPauseWithNative();
        verify(mNatives, never()).onPause(mProfile);

        secondWindow.onPauseWithNative();
        verify(mNatives).onPause(mProfile);
    }
}
