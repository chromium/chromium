// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.BackForwardTransitionAnimationManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

/** Tests the Tab gesture ownership checks independently of native animation code. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabOnBackGestureHandlerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private BackForwardTransitionAnimationManager mManager;
    private UserDataHost mUserDataHost;
    private TabOnBackGestureHandler mHandler;

    @Before
    public void setUp() {
        mUserDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getBackForwardTransitionAnimationManager()).thenReturn(mManager);
        mHandler = TabOnBackGestureHandler.from(mTab);
    }

    private void start() {
        mHandler.onBackStarted(0.2f, BackGestureEventSwipeEdge.LEFT, false, true);
        clearInvocations(mManager);
    }

    @Test
    public void reusedForSameTab() {
        assertSame(mHandler, TabOnBackGestureHandler.from(mTab));
    }

    @Test
    public void progressWithoutStartReturnsControl() {
        assertFalse(mHandler.onBackProgressed(0.5f, BackGestureEventSwipeEdge.LEFT, false, true));
        mHandler.onBackInvoked(true);
        mHandler.onBackCancelled(true);
        verifyNoInteractions(mManager);
    }

    @Test
    public void matchingGestureInvokesOnlyOnce() {
        start();
        assertTrue(mHandler.onBackProgressed(0.5f, BackGestureEventSwipeEdge.LEFT, false, true));
        mHandler.onBackInvoked(true);
        mHandler.onBackInvoked(true);
        verify(mManager).onGestureProgressed(0.5f, true);
        verify(mManager).onGestureInvoked(true);
        verifyNoMoreInteractions(mManager);
    }

    @Test
    public void edgeMismatchPreservesOriginalGesture() {
        start();
        assertFalse(mHandler.onBackProgressed(0.5f, BackGestureEventSwipeEdge.RIGHT, true, true));
        verifyNoInteractions(mManager);
        assertTrue(mHandler.onBackProgressed(0.6f, BackGestureEventSwipeEdge.LEFT, false, true));
        mHandler.onBackCancelled(true);
        verify(mManager).onGestureProgressed(0.6f, true);
        verify(mManager).onGestureCancelled(true);
        verifyNoMoreInteractions(mManager);
    }

    @Test
    public void secondStartCancelsBeforeStarting() {
        start();
        mHandler.onBackStarted(0.3f, BackGestureEventSwipeEdge.RIGHT, true, false);
        var order = inOrder(mManager);
        order.verify(mManager).onGestureCancelled(false);
        order.verify(mManager).onGestureStarted(0.3f, BackGestureEventSwipeEdge.RIGHT, true, false);
    }

    @Test
    public void swappedWebContentsGetsNoGestureEvents() {
        start();
        WebContents replacement = mock(WebContents.class);
        BackForwardTransitionAnimationManager replacementManager =
                mock(BackForwardTransitionAnimationManager.class);
        when(replacement.getBackForwardTransitionAnimationManager()).thenReturn(replacementManager);
        when(mTab.getWebContents()).thenReturn(replacement);
        assertFalse(mHandler.onBackProgressed(0.5f, BackGestureEventSwipeEdge.LEFT, false, true));
        mHandler.onBackInvoked(true);
        mHandler.onBackCancelled(true);
        mHandler.destroy();
        verifyNoInteractions(mManager, replacementManager);
    }

    @Test
    public void destroyedWebContentsGetsNoGestureEvents() {
        start();
        when(mWebContents.isDestroyed()).thenReturn(true);
        assertFalse(mHandler.onBackProgressed(0.5f, BackGestureEventSwipeEdge.LEFT, false, true));
        mHandler.onBackCancelled(true);
        verifyNoInteractions(mManager);
    }

    @Test
    public void replacedManagerDoesNotReceiveTerminalEvent() {
        start();
        var replacementManager = mock(BackForwardTransitionAnimationManager.class);
        when(mWebContents.getBackForwardTransitionAnimationManager())
                .thenReturn(replacementManager);
        assertFalse(mHandler.onBackProgressed(0.5f, BackGestureEventSwipeEdge.LEFT, false, true));
        mHandler.onBackInvoked(true);
        verifyNoInteractions(mManager, replacementManager);
    }

    @Test
    public void userDataHostDestructionCancelsActiveAnimationAndDisablesHandler() {
        start();
        when(mManager.getCurrentAnimationStage()).thenReturn(AnimationStage.OTHER);
        // TabImpl destroys its UserDataHost before releasing or destroying its WebContents.
        mUserDataHost.destroy();
        mHandler.destroy();
        mHandler.onBackStarted(0.2f, BackGestureEventSwipeEdge.LEFT, false, true);
        assertFalse(mHandler.onBackProgressed(0.5f, BackGestureEventSwipeEdge.LEFT, false, true));
        mHandler.onBackInvoked(true);
        mHandler.onBackCancelled(true);
        verify(mManager).getCurrentAnimationStage();
        verify(mManager).onGestureCancelled(true);
        verifyNoMoreInteractions(mManager);
    }

    @Test
    public void destroyDoesNotCancelAbortedAnimation() {
        start();
        when(mManager.getCurrentAnimationStage()).thenReturn(AnimationStage.NONE);
        mHandler.destroy();
        verify(mManager).getCurrentAnimationStage();
        verifyNoMoreInteractions(mManager);
    }
}
