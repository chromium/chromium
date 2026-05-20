// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;

/** Tests for {@link ExclusiveAccessContext}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExclusiveAccessContextTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private ExclusiveAccessContext.Natives mExclusiveAccessContextJni;

    @Before
    public void setUp() {
        ExclusiveAccessContextJni.setInstanceForTesting(mExclusiveAccessContextJni);
    }

    @Test
    @SmallTest
    public void testTouchDownNotifiesNative() {
        long nativePtr = 123L;
        ActivityTabProvider activityTabProvider = new ActivityTabProvider();
        ExclusiveAccessContext context =
                ExclusiveAccessContext.create(
                        nativePtr, mContext, mFullscreenManager, activityTabProvider);

        // Simulate touch down on the active tab.
        context.mActiveTabObserver.onTouchDown();

        verify(mExclusiveAccessContextJni).onExclusiveAccessUserInput(nativePtr);
    }
}
