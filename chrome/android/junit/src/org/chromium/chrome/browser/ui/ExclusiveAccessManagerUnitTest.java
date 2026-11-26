// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.KeyEvent;

import org.jni_zero.JniUniquePtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/** Unit tests for {@link ExclusiveAccessManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExclusiveAccessManagerUnitTest {
    private static final FullscreenOptions SAVED_OPTIONS =
            new FullscreenOptions(
                    /* showNavigationBar= */ false, /* showStatusBar= */ true, /* displayId= */ 7);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ExclusiveAccessManager.Natives mJni;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private RenderFrameHost mMainFrame;
    @Mock private RenderFrameHost mRenderFrameHost;

    private final SettableNonNullObservableSupplier<Boolean> mPersistentModeSupplier =
            ObservableSuppliers.createNonNull(false);
    private JniUniquePtr<ExclusiveAccessManager.NativeExclusiveAccessManagerAndroid> mNativePtr;
    private ExclusiveAccessManager mManager;

    @Before
    public void setUp() {
        ExclusiveAccessManagerJni.setInstanceForTesting(mJni);
        // Created on init() so that destroy() releases it in every test.
        when(mJni.init(any(), any(), any(), any()))
                .thenAnswer(
                        invocation -> {
                            mNativePtr = JniUniquePtr.createForTesting(1L);
                            return mNativePtr;
                        });
        when(mFullscreenManager.getPersistentFullscreenModeSupplier())
                .thenReturn(mPersistentModeSupplier);
        when(mWebContents.getMainFrame()).thenReturn(mMainFrame);
        mManager =
                new ExclusiveAccessManager(
                        mFullscreenManager, /* desktopWindowStateManager= */ null);
    }

    @After
    public void tearDown() {
        mManager.destroy();
    }

    private static Bundle createSavedState() {
        Bundle bundle = new Bundle();
        bundle.putParcelable(ExclusiveAccessManager.LATEST_FULLSCREEN_OPTIONS, SAVED_OPTIONS);
        return bundle;
    }

    private void initialize() {
        mManager.initialize(mTabModelSelector, mWindowAndroid, new ActivityTabProvider());
    }

    private TabModelObserver initializeAndCaptureTabModelObserver() {
        initialize();
        ArgumentCaptor<TabModelObserver> observerCaptor =
                ArgumentCaptor.forClass(TabModelObserver.class);
        verify(mTabModelSelector).addObserverToAllModels(observerCaptor.capture());
        return observerCaptor.getValue();
    }

    private void verifyNoFullscreenEntered() {
        verify(mJni, never()).forceActiveTab(any(), any());
        verify(mJni, never())
                .enterFullscreenModeForTab(any(), any(), anyBoolean(), anyBoolean(), anyLong());
    }

    @Test
    public void testSetFullscreenPendingState_reportsPendingBeforeInitialize() {
        mManager.setFullscreenPendingState(createSavedState());

        assertTrue(mManager.isFullscreenForTabOrPending(null));
    }

    @Test
    public void testSetFullscreenPendingState_nullBundle_notPending() {
        mManager.setFullscreenPendingState(null);

        assertFalse(mManager.isFullscreenForTabOrPending(null));
    }

    @Test
    public void testSetFullscreenPendingState_bundleWithoutOptions_notPending() {
        mManager.setFullscreenPendingState(new Bundle());

        assertFalse(mManager.isFullscreenForTabOrPending(null));
    }

    @Test
    public void testInitialize_notPending_doesNotEnterFullscreen() {
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mTab.getWebContents()).thenReturn(mWebContents);

        initialize();

        verifyNoFullscreenEntered();
    }

    @Test
    public void testInitialize_pendingWithWebContents_reentersSavedFullscreen() {
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        mManager.setFullscreenPendingState(createSavedState());

        initialize();

        verify(mJni).forceActiveTab(mNativePtr, mTab);
        verify(mJni)
                .enterFullscreenModeForTab(
                        mNativePtr,
                        mMainFrame,
                        SAVED_OPTIONS.showNavigationBar,
                        SAVED_OPTIONS.showStatusBar,
                        SAVED_OPTIONS.displayId);
        assertFalse(mManager.isFullscreenForTabOrPending(null));

        Bundle outState = new Bundle();
        mManager.saveFullscreenState(outState);
        assertEquals(
                SAVED_OPTIONS,
                outState.getParcelable(ExclusiveAccessManager.LATEST_FULLSCREEN_OPTIONS));
    }

    @Test
    public void testInitialize_pendingWithoutWebContents_doesNotEnterFullscreen() {
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        when(mTab.getWebContents()).thenReturn(null);
        mManager.setFullscreenPendingState(createSavedState());

        initialize();

        verifyNoFullscreenEntered();
        assertFalse(mManager.isFullscreenForTabOrPending(null));
    }

    @Test
    public void testInitialize_pendingWithoutCurrentTab_doesNotEnterFullscreen() {
        when(mTabModelSelector.getCurrentTab()).thenReturn(null);
        mManager.setFullscreenPendingState(createSavedState());

        initialize();

        verifyNoFullscreenEntered();
        assertFalse(mManager.isFullscreenForTabOrPending(null));
    }

    @Test
    public void testInitialize_pendingWithoutCurrentTab_dropsSavedOptions() {
        when(mTabModelSelector.getCurrentTab()).thenReturn(null);
        mManager.setFullscreenPendingState(createSavedState());

        initialize();

        Bundle outState = new Bundle();
        mManager.saveFullscreenState(outState);
        assertFalse(outState.containsKey(ExclusiveAccessManager.LATEST_FULLSCREEN_OPTIONS));
    }

    @Test
    public void testInitialize_pendingWithoutCurrentTab_laterIdenticalRequestNotDeduplicated() {
        when(mTabModelSelector.getCurrentTab()).thenReturn(null);
        mManager.setFullscreenPendingState(createSavedState());
        initialize();

        mManager.enterFullscreenModeForTab(
                mRenderFrameHost,
                new FullscreenOptions(
                        SAVED_OPTIONS.showNavigationBar,
                        SAVED_OPTIONS.showStatusBar,
                        SAVED_OPTIONS.displayId));

        verify(mJni)
                .enterFullscreenModeForTab(
                        mNativePtr,
                        mRenderFrameHost,
                        SAVED_OPTIONS.showNavigationBar,
                        SAVED_OPTIONS.showStatusBar,
                        SAVED_OPTIONS.displayId);
    }

    @Test
    public void testWillCloseTabs_notifiesNativeAndDropsSavedOptions() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        TabModelObserver observer = initializeAndCaptureTabModelObserver();
        mManager.enterFullscreenModeForTab(mRenderFrameHost, SAVED_OPTIONS);

        observer.willCloseTabs(List.of(mTab), /* isAllTabs= */ false, /* allowUndo= */ true);

        verify(mJni).onTabClosing(mNativePtr, mWebContents);
        Bundle outState = new Bundle();
        mManager.saveFullscreenState(outState);
        assertFalse(outState.containsKey(ExclusiveAccessManager.LATEST_FULLSCREEN_OPTIONS));
    }

    @Test
    public void testDestroy_unregistersTabModelObserverBeforeDestroyingNative() {
        doAnswer(
                        invocation -> {
                            when(mJni.hasExclusiveAccess(mNativePtr)).thenReturn(true);
                            assertTrue(mManager.hasExclusiveAccess());
                            return null;
                        })
                .when(mTabModelSelector)
                .removeObserverFromAllModels(any());
        TabModelObserver observer = initializeAndCaptureTabModelObserver();

        mManager.destroy();
        verify(mTabModelSelector).removeObserverFromAllModels(observer);
        assertFalse(mManager.hasExclusiveAccess());

        clearInvocations(mTabModelSelector);
        mManager.destroy();
        verify(mTabModelSelector, never()).removeObserverFromAllModels(any());

        List<Tab> tabs = List.of(mTab);
        assertThrows(
                AssertionError.class,
                () -> observer.willCloseTabs(tabs, /* isAllTabs= */ false, /* allowUndo= */ true));
    }

    @Test
    public void testDestroy_beforeInitialize_doesNotUnregisterTabModelObserver() {
        mManager.destroy();

        verify(mTabModelSelector, never()).removeObserverFromAllModels(any());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testPreHandleEscapeKeyEvent_featureDisabled_ignored() {
        initialize();

        KeyEvent escDown = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE);
        KeyEvent escUp = new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ESCAPE);
        assertFalse(mManager.preHandleEscapeKeyEvent(escDown));
        assertFalse(mManager.preHandleEscapeKeyEvent(escUp));

        // Feature flag is off, so nothing should be forwarded to native.
        verify(mJni, never()).preHandleEscapeKeyEvent(any(), anyBoolean(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testPreHandleEscapeKeyEvent_nonEscapeKey_ignored() {
        initialize();

        KeyEvent aDown = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A);
        assertFalse(mManager.preHandleEscapeKeyEvent(aDown));

        // Only Esc is handled here; other keys should be ignored.
        verify(mJni, never()).preHandleEscapeKeyEvent(any(), anyBoolean(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testPreHandleEscapeKeyEvent_notInExclusiveAccess_skipsKeyDownAndForwardsKeyUp() {
        initialize();

        KeyEvent escDown = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE);
        KeyEvent escUp = new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ESCAPE);
        assertFalse(mManager.preHandleEscapeKeyEvent(escDown));
        assertFalse(mManager.preHandleEscapeKeyEvent(escUp));

        verify(mJni, never())
                .preHandleEscapeKeyEvent(mNativePtr, /* isKeyDown= */ true, /* metaState= */ 0);
        verify(mJni, times(1))
                .preHandleEscapeKeyEvent(mNativePtr, /* isKeyDown= */ false, /* metaState= */ 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testPreHandleEscapeKeyEvent_escapeDownAndUp_forwardsToNativeAndSkipsRepeats() {
        initialize();
        mPersistentModeSupplier.set(true);

        // Simulates long-pressing plain Esc: initial press (repeat=0), an auto-repeat event while
        // held (repeat=1), and releasing the key (ACTION_UP).
        KeyEvent escDown =
                new KeyEvent(
                        /* downTime= */ 100L,
                        /* eventTime= */ 100L,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_ESCAPE,
                        /* repeat= */ 0,
                        /* metaState= */ 0);
        KeyEvent escRepeat =
                new KeyEvent(
                        /* downTime= */ 100L,
                        /* eventTime= */ 500L,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_ESCAPE,
                        /* repeat= */ 1,
                        /* metaState= */ 0);
        KeyEvent escUp =
                new KeyEvent(
                        /* downTime= */ 100L,
                        /* eventTime= */ 800L,
                        KeyEvent.ACTION_UP,
                        KeyEvent.KEYCODE_ESCAPE,
                        /* repeat= */ 0,
                        /* metaState= */ 0);

        assertFalse(mManager.preHandleEscapeKeyEvent(escDown));
        assertFalse(mManager.preHandleEscapeKeyEvent(escRepeat));
        assertFalse(mManager.preHandleEscapeKeyEvent(escUp));

        // Initial key-down was forwarded once (auto-repeat was skipped) and key-up was forwarded
        // once.
        verify(mJni, times(1))
                .preHandleEscapeKeyEvent(mNativePtr, /* isKeyDown= */ true, /* metaState= */ 0);
        verify(mJni, times(1))
                .preHandleEscapeKeyEvent(mNativePtr, /* isKeyDown= */ false, /* metaState= */ 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testPreHandleEscapeKeyEvent_forwardsMetaState() {
        initialize();
        mPersistentModeSupplier.set(true);

        // Modifier keys (e.g. Ctrl+Esc) must be forwarded so native knows not to treat modified Esc
        // as an unmodified long-press exit.
        KeyEvent ctrlEscDown =
                new KeyEvent(
                        /* downTime= */ 100L,
                        /* eventTime= */ 100L,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_ESCAPE,
                        /* repeat= */ 0,
                        /* metaState= */ KeyEvent.META_CTRL_ON);
        assertFalse(mManager.preHandleEscapeKeyEvent(ctrlEscDown));

        verify(mJni, times(1))
                .preHandleEscapeKeyEvent(mNativePtr, /* isKeyDown= */ true, KeyEvent.META_CTRL_ON);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testPreHandleEscapeKeyEvent_nativeConsumesKey_updatesExclusiveAccessState() {
        initialize();
        mPersistentModeSupplier.set(true);
        when(mJni.preHandleEscapeKeyEvent(mNativePtr, /* isKeyDown= */ true, /* metaState= */ 0))
                .thenReturn(true);
        when(mJni.hasExclusiveAccess(mNativePtr)).thenReturn(false);

        KeyEvent escDown = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE);
        assertTrue(mManager.preHandleEscapeKeyEvent(escDown));
        assertFalse(mManager.getExclusiveAccessStateSupplier().get());
    }
}
