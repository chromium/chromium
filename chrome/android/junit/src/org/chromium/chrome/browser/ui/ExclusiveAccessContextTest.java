// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.ui.base.WindowAndroid;

/** Tests for {@link ExclusiveAccessContext}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExclusiveAccessContextTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private ExclusiveAccessContext.Natives mExclusiveAccessContextJni;
    private final UnownedUserDataHost mUnownedUserDataHost = new UnownedUserDataHost();

    @Before
    public void setUp() {
        ExclusiveAccessContextJni.setInstanceForTesting(mExclusiveAccessContextJni);
    }

    @After
    public void tearDown() {
        SnackbarManagerProvider.detach(mSnackbarManager);
    }

    @Test
    @SmallTest
    public void testTouchDownNotifiesNative() {
        long nativePtr = 123L;
        ActivityTabProvider activityTabProvider = new ActivityTabProvider();
        ExclusiveAccessContext context =
                ExclusiveAccessContext.create(
                        nativePtr, mWindowAndroid, mFullscreenManager, activityTabProvider);

        // Simulate touch down on the active tab.
        context.mActiveTabObserver.onTouchDown();

        verify(mExclusiveAccessContextJni).onExclusiveAccessUserInput(nativePtr);
    }

    @Test
    @SmallTest
    public void testGetsSnackbarManagerFromWindowAndroid() {
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUnownedUserDataHost);
        SnackbarManagerProvider.attach(mWindowAndroid, mSnackbarManager);
        ExclusiveAccessContext context =
                ExclusiveAccessContext.create(
                        123L, mWindowAndroid, mFullscreenManager, new ActivityTabProvider());

        assertSame(mSnackbarManager, context.getSnackbarManager());
    }
}
