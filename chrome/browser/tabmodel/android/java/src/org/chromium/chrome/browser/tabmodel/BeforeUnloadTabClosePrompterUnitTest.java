// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link BeforeUnloadTabClosePrompter}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_BEFORE_UNLOAD_SUPPORT)
public class BeforeUnloadTabClosePrompterUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private NavigationHandle mNavigation;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Runnable mOnProceed;
    @Mock private Runnable mOnCancel;

    @Captor private ArgumentCaptor<TabObserver> mObserverCaptor;

    private BeforeUnloadTabClosePrompter mPrompter;
    private TabClosureParams mParams;

    @Before
    public void setUp() {
        DeviceInfo.setIsDesktopForTesting(true);

        lenient().when(mTab.getWebContents()).thenReturn(mWebContents);
        lenient().when(mWebContents.needToFireBeforeUnload()).thenReturn(true);

        mPrompter = new BeforeUnloadTabClosePrompter();
        mParams = TabClosureParams.closeTab(mTab).build();
    }

    /** Runs the prompt and returns the observer it attached to the tab. */
    private TabObserver promptAndCaptureObserver() {
        assertTrue(mPrompter.prompt(mParams, mTab, mOnProceed, mOnCancel));
        verify(mTab).addObserver(mObserverCaptor.capture());
        return mObserverCaptor.getValue();
    }

    // The order is load-bearing: a completion can arrive synchronously, so the mark has to be set
    // and the observer attached before the dispatch goes out.
    @Test
    public void testPrompt_SuppressesAutoCloseAndDispatches() {
        assertTrue(mPrompter.prompt(mParams, mTab, mOnProceed, mOnCancel));

        InOrder inOrder = inOrder(mTab, mWebContents);
        inOrder.verify(mTab).setSuppressBeforeUnloadAutoClose(true);
        inOrder.verify(mTab).addObserver(any());
        inOrder.verify(mWebContents).dispatchBeforeUnload(false);
        verifyNoInteractions(mOnProceed, mOnCancel);
    }

    @Test
    public void testPrompt_PageAgrees_ProceedsOffTheNativeStack() {
        TabObserver observer = promptAndCaptureObserver();

        observer.onBeforeUnloadFired(mTab, /* proceed= */ true);
        // The answer is acted on in a posted task, so nothing has run yet.
        verifyNoInteractions(mOnProceed, mOnCancel);

        ShadowLooper.idleMainLooper();
        verify(mOnProceed).run();
        verifyNoInteractions(mOnCancel);
        verify(mTab).removeObserver(observer);
    }

    @Test
    public void testPrompt_PageRefuses_Cancels() {
        TabObserver observer = promptAndCaptureObserver();

        observer.onBeforeUnloadFired(mTab, /* proceed= */ false);
        ShadowLooper.idleMainLooper();

        verify(mOnCancel).run();
        verifyNoInteractions(mOnProceed);
        verify(mTab).removeObserver(observer);
    }

    @Test
    public void testPrompt_TabDestroyed_Cancels() {
        TabObserver observer = promptAndCaptureObserver();

        observer.onDestroyed(mTab);
        ShadowLooper.idleMainLooper();

        verify(mOnCancel).run();
        verifyNoInteractions(mOnProceed);
        verify(mTab).removeObserver(observer);
    }

    // A crashed page has no unsaved state left to protect, and refusing here would leave the user
    // unable to close a crashed tab.
    @Test
    public void testPrompt_RendererCrashes_Proceeds() {
        TabObserver observer = promptAndCaptureObserver();

        observer.onCrash(mTab);
        ShadowLooper.idleMainLooper();

        verify(mOnProceed).run();
        verifyNoInteractions(mOnCancel);
        verify(mTab).removeObserver(observer);
    }

    // TabObserver's default onActivityAttachmentChanged detaches the observer when the tab leaves
    // its window, so without an override the closure would wait forever.
    @Test
    public void testPrompt_TabDetachedFromWindow_Cancels() {
        TabObserver observer = promptAndCaptureObserver();

        observer.onActivityAttachmentChanged(mTab, /* window= */ null);
        ShadowLooper.idleMainLooper();

        verify(mOnCancel).run();
        verifyNoInteractions(mOnProceed);
        verify(mTab).removeObserver(observer);
    }

    // Reattaching leaves the dispatch alone.
    @Test
    public void testPrompt_TabAttachedToWindow_DoesNotResolve() {
        TabObserver observer = promptAndCaptureObserver();

        observer.onActivityAttachmentChanged(mTab, mWindowAndroid);
        ShadowLooper.idleMainLooper();

        verifyNoInteractions(mOnProceed);
        verifyNoInteractions(mOnCancel);
        verify(mTab, never()).removeObserver(observer);
    }

    // A new primary page abandons the dispatch without a completion.
    @Test
    public void testPrompt_PrimaryPageChanges_Cancels() {
        TabObserver observer = promptAndCaptureObserver();
        when(mNavigation.hasCommitted()).thenReturn(true);
        when(mNavigation.isSameDocument()).thenReturn(false);

        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);
        ShadowLooper.idleMainLooper();

        verify(mOnCancel).run();
        verifyNoInteractions(mOnProceed);
        verify(mTab).removeObserver(observer);
    }

    // A same-document navigation leaves the page in place, so the dispatch still completes and this
    // observer has to stay for it. Cancelling here would abandon a closure that is still going to
    // get an answer, and the native suppression mark -- which deliberately survives the same
    // navigation -- would then be spent with nobody waiting on it.
    @Test
    public void testPrompt_SameDocumentNavigation_DoesNotResolve() {
        TabObserver observer = promptAndCaptureObserver();
        when(mNavigation.hasCommitted()).thenReturn(true);
        when(mNavigation.isSameDocument()).thenReturn(true);

        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);
        ShadowLooper.idleMainLooper();

        verifyNoInteractions(mOnProceed, mOnCancel);
        verify(mTab, never()).removeObserver(any());

        // Still armed: the completion it was waiting for still arrives.
        observer.onBeforeUnloadFired(mTab, /* proceed= */ true);
        ShadowLooper.idleMainLooper();
        verify(mOnProceed).run();
    }

    // A navigation that never commits -- aborted, a 204, a download -- still reports here. The page
    // stays and the dispatch still completes, so this observer has to stay for it. Cancelling here
    // would abandon a closure that is still going to get an answer, and the native suppression mark
    // -- which no uncommitted navigation drops -- would then be spent with nobody waiting on it.
    @Test
    public void testPrompt_UncommittedNavigation_DoesNotResolve() {
        TabObserver observer = promptAndCaptureObserver();
        when(mNavigation.hasCommitted()).thenReturn(false);
        when(mNavigation.isSameDocument()).thenReturn(false);

        observer.onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigation);
        ShadowLooper.idleMainLooper();

        verifyNoInteractions(mOnProceed, mOnCancel);
        verify(mTab, never()).removeObserver(any());

        // Still armed: the completion it was waiting for still arrives.
        observer.onBeforeUnloadFired(mTab, /* proceed= */ true);
        ShadowLooper.idleMainLooper();
        verify(mOnProceed).run();
    }

    // The WebContents that would answer has left the tab, taking the dispatch with it.
    @Test
    public void testPrompt_WebContentsLeavesTab_Cancels() {
        TabObserver observer = promptAndCaptureObserver();
        when(mTab.getWebContents()).thenReturn(null);

        observer.onContentChanged(mTab);
        ShadowLooper.idleMainLooper();

        verify(mOnCancel).run();
        verifyNoInteractions(mOnProceed);
        verify(mTab).removeObserver(observer);
    }

    // onContentChanged also fires for changes that leave the dispatch alone -- a custom view, a
    // native page being shown or hidden. Those must not abandon a closure that is still going to
    // get an answer.
    @Test
    public void testPrompt_ContentChangedWithSameWebContents_DoesNotResolve() {
        TabObserver observer = promptAndCaptureObserver();

        observer.onContentChanged(mTab);
        ShadowLooper.idleMainLooper();

        verifyNoInteractions(mOnProceed, mOnCancel);
        verify(mTab, never()).removeObserver(any());
    }

    @Test
    public void testPrompt_SecondCompletion_IsIgnored() {
        TabObserver observer = promptAndCaptureObserver();

        observer.onBeforeUnloadFired(mTab, /* proceed= */ true);
        observer.onBeforeUnloadFired(mTab, /* proceed= */ false);
        observer.onDestroyed(mTab);
        ShadowLooper.idleMainLooper();

        verify(mOnProceed).run();
        verifyNoInteractions(mOnCancel);
    }

    @Test
    public void testPrompt_ClosureDisallowsUnloadHandlers_NotAsked() {
        TabClosureParams params =
                TabClosureParams.closeTab(mTab).allowUnloadHandlers(false).build();

        assertFalse(mPrompter.prompt(params, mTab, mOnProceed, mOnCancel));
        verifyNothingDispatched();
    }

    @Test
    public void testPrompt_NotDesktop_NotAsked() {
        DeviceInfo.setIsDesktopForTesting(false);

        assertFalse(mPrompter.prompt(mParams, mTab, mOnProceed, mOnCancel));
        verifyNothingDispatched();
    }

    @Test
    public void testPrompt_DestroyedTab_NotAsked() {
        when(mTab.isDestroyed()).thenReturn(true);

        assertFalse(mPrompter.prompt(mParams, mTab, mOnProceed, mOnCancel));
        verifyNothingDispatched();
    }

    @Test
    public void testPrompt_NoWebContents_NotAsked() {
        when(mTab.getWebContents()).thenReturn(null);

        assertFalse(mPrompter.prompt(mParams, mTab, mOnProceed, mOnCancel));
        verifyNothingDispatched();
    }

    @Test
    public void testPrompt_NoBeforeUnloadHandler_NotAsked() {
        // A page with only unload, pagehide or visibilitychange handlers.
        lenient().when(mWebContents.needToFireBeforeUnloadOrUnloadEvents()).thenReturn(true);
        when(mWebContents.needToFireBeforeUnload()).thenReturn(false);

        assertFalse(mPrompter.prompt(mParams, mTab, mOnProceed, mOnCancel));
        verifyNothingDispatched();
    }

    /** Asserts the tab was left entirely alone: no mark, no dispatch, no observer. */
    private void verifyNothingDispatched() {
        verify(mTab, never()).setSuppressBeforeUnloadAutoClose(anyBoolean());
        verify(mWebContents, never()).dispatchBeforeUnload(anyBoolean());
        verify(mTab, never()).addObserver(any());
        verifyNoInteractions(mOnProceed, mOnCancel);
    }
}
