// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Rect;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature.InitInfo;

/** Unit tests for {@link GlicSplitButtonDelegateBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicSplitButtonDelegateBridgeUnitTest {
    private static final long BROWSER_WINDOW_PTR = 100L;
    private static final long NATIVE_BRIDGE_PTR = 200L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private GlicSplitButtonDelegate mDelegateMock;
    @Mock private GlicSplitButtonDelegateBridge.Natives mNativesMock;

    private GlicSplitButtonDelegateBridge mBridge;

    @Before
    public void setUp() {
        GlicSplitButtonDelegateBridgeJni.setInstanceForTesting(mNativesMock);
        mBridge = new GlicSplitButtonDelegateBridge(mDelegateMock);
    }

    private static InitInfo createInitInfo(long nativeBrowserWindowPtr) {
        return new InitInfo(
                nativeBrowserWindowPtr,
                /* isVisible= */ true,
                /* boundsInPx= */ new Rect(),
                /* boundsInDp= */ new Rect(),
                /* displayId= */ 0);
    }

    @Test
    public void testLifecycleAndJavaToNativeForwarding() {
        when(mNativesMock.create(BROWSER_WINDOW_PTR, mBridge)).thenReturn(NATIVE_BRIDGE_PTR);
        // Zero browser window pointer should be a no-op.
        mBridge.onAddedToTask(createInitInfo(0L));
        mBridge.onGlicActorButtonClicked();
        verify(mNativesMock, never()).onGlicActorButtonClicked(anyLong());

        // Valid browser window pointer initializes native bridge and forwards events.
        mBridge.onAddedToTask(createInitInfo(BROWSER_WINDOW_PTR));
        verify(mNativesMock).create(BROWSER_WINDOW_PTR, mBridge);

        mBridge.onNudgeActivity(GlicNudgeActivity.NUDGE_SHOWN);
        verify(mNativesMock).onNudgeActivity(NATIVE_BRIDGE_PTR, GlicNudgeActivity.NUDGE_SHOWN);

        mBridge.onTaskRowClicked(42);
        verify(mNativesMock).onTaskRowClicked(NATIVE_BRIDGE_PTR, 42);

        mBridge.onGlicActorButtonClicked();
        verify(mNativesMock).onGlicActorButtonClicked(NATIVE_BRIDGE_PTR);

        mBridge.onActorTaskListBubbleDismissed();
        verify(mNativesMock).onActorTaskListBubbleDismissed(NATIVE_BRIDGE_PTR);

        // Removing the feature destroys the native bridge and stops forwarding.
        mBridge.onFeatureRemoved();
        verify(mNativesMock).destroy(NATIVE_BRIDGE_PTR);

        mBridge.onTaskRowClicked(99);
        verify(mNativesMock, never()).onTaskRowClicked(anyLong(), eq(99));
    }
}
