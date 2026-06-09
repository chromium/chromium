// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.content_public.browser.ImmersivePlaybackConfirmationStatus;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ImmersivePlaybackSnackbarController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImmersivePlaybackSnackbarControllerTest {
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Tab mTab;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private ImmersivePlaybackConfirmationCallback mCallback;

    private Context mContext;
    private ImmersivePlaybackSnackbarController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mController =
                new ImmersivePlaybackSnackbarController(
                        mContext,
                        () -> mSnackbarManager,
                        () -> mModalDialogManager,
                        mTab,
                        mFullscreenManager);
    }

    @Test
    public void testShow_RegistersObservers() {
        mController.show(mCallback, 0);
        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
        verify(mTab).addObserver(any());
        verify(mFullscreenManager).addObserver(any());
    }

    @Test
    public void testActionClicks_OpensDialog() {
        mController.show(mCallback, 0);

        mController.onAction(null);

        verify(mTab).removeObserver(any());
        verify(mFullscreenManager).removeObserver(any());
        verify(mModalDialogManager)
                .showDialog(
                        any(PropertyModel.class),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));
    }

    @Test
    public void testDismissNoAction_Declines() {
        mController.show(mCallback, 0);

        mController.onDismissNoAction(null);

        verify(mTab).removeObserver(any());
        verify(mFullscreenManager).removeObserver(any());
        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.DECLINED,
                        ImmersiveStereoMode.MONO,
                        ImmersiveProjectionType.QUAD);
    }

    @Test
    public void testDismiss_DismissesSnackbar() {
        mController.show(mCallback, 0);
        clearInvocations(mSnackbarManager);
        mController.dismiss();
        verify(mSnackbarManager).dismissSnackbars(eq(mController));
    }

    @Test
    public void testDismiss_DismissesDialog() {
        mController.show(mCallback, 0);
        mController.onAction(null); // consumes snackbar, shows dialog
        mController.dismiss();
        verify(mModalDialogManager).dismissDialog(any(), any(Integer.class));
    }

    @Test
    public void testShow_NullModalDialogManager_FailsImmediately() {
        mController =
                new ImmersivePlaybackSnackbarController(
                        mContext, () -> mSnackbarManager, () -> null, mTab, mFullscreenManager);

        mController.show(mCallback, 0);

        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.FAILED,
                        ImmersiveStereoMode.MONO,
                        ImmersiveProjectionType.QUAD);
        verify(mSnackbarManager, never()).showSnackbar(any());
        verify(mTab, never()).addObserver(any());
    }

    @Test
    public void testShow_DuplicateCalls_GuardAddsObserversOnce() {
        mController.show(mCallback, 0);
        verify(mTab).addObserver(any());
        verify(mFullscreenManager).addObserver(any());

        mController.show(mCallback, 0);

        // Consecutive call to show() will unregister and re-register the observers.
        verify(mTab).removeObserver(any());
        verify(mFullscreenManager).removeObserver(any());
        verify(mTab, times(2)).addObserver(any());
        verify(mFullscreenManager, times(2)).addObserver(any());
    }

    @Test
    public void testObservers_TriggerEvents_DismissesSnackbar() {
        ArgumentCaptor<TabObserver> tabCaptor = ArgumentCaptor.forClass(TabObserver.class);
        ArgumentCaptor<FullscreenManager.Observer> fsCaptor =
                ArgumentCaptor.forClass(FullscreenManager.Observer.class);

        // 1. Test page load started
        mController.show(mCallback, 0);
        verify(mTab).addObserver(tabCaptor.capture());
        clearInvocations(mSnackbarManager);
        tabCaptor.getValue().onPageLoadStarted(mTab, null);
        verify(mSnackbarManager).dismissSnackbars(eq(mController));

        // 2. Test content changed
        mController.show(mCallback, 0);
        clearInvocations(mSnackbarManager);
        tabCaptor.getValue().onContentChanged(mTab);
        verify(mSnackbarManager).dismissSnackbars(eq(mController));

        // 3. Test exit fullscreen
        mController.show(mCallback, 0);
        verify(mFullscreenManager, times(3)).addObserver(fsCaptor.capture());
        clearInvocations(mSnackbarManager);
        fsCaptor.getValue().onExitFullscreen(mTab);
        verify(mSnackbarManager).dismissSnackbars(eq(mController));
    }

    @Test
    public void testShow_WithDelay_PostsSnackbar() {
        mController.show(mCallback, 1000);
        verify(mSnackbarManager, never()).showSnackbar(any());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testShow_WithDelay_DismissCancelsTask() {
        mController.show(mCallback, 1000);
        verify(mSnackbarManager, never()).showSnackbar(any());

        mController.dismiss();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mSnackbarManager, never()).showSnackbar(any());
    }
}
