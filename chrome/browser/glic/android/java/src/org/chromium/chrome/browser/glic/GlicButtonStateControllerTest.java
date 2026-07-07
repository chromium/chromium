// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.glic.GlicButtonStateController.ButtonState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

import java.util.Arrays;
import java.util.Collections;

/** Unit tests for {@link GlicButtonStateController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicButtonStateControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorService;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    @Mock private ChromeAndroidTask mTask;
    @Mock private GlicButtonStateController.Listener mListener;
    @Mock private GlicKeyedServiceFactory.Natives mGlicKeyedServiceFactoryJniMock;

    private GlicButtonStateController mController;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        ActorKeyedServiceFactory.setForTesting(mActorService);
        GlicKeyedServiceFactoryJni.setInstanceForTesting(mGlicKeyedServiceFactoryJniMock);
        when(mGlicKeyedServiceFactoryJniMock.getForProfile(mProfile)).thenReturn(mGlicKeyedService);

        mBrowserControlsVisibilityDelegate =
                new BrowserStateBrowserControlsVisibilityDelegate(
                        org.chromium.base.supplier.ObservableSuppliers.alwaysFalse());
        when(mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate())
                .thenReturn(mBrowserControlsVisibilityDelegate);

        mController =
                new GlicButtonStateController(
                        mActivity, mListener, () -> mTask, mBrowserControlsVisibilityManager);
    }

    @Test
    public void testInitialState() {
        Assert.assertEquals(ButtonState.DEFAULT, mController.getButtonState());
        Assert.assertFalse(mController.isPanelOpen());
    }

    @Test
    public void testUpdateObservations() {
        mController.updateObservations(mProfile);

        verify(mActorService).addObserver(mController);
        verify(mGlicKeyedService).addGlobalShowHideObserver(mController);
    }

    @Test
    public void testOnTaskStateChanged() {
        mController.updateObservations(mProfile);

        ActorTask task = mock(ActorTask.class);
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        // Trigger state change to WORKING.
        when(task.getState()).thenReturn(ActorTaskState.ACTING);
        mController.onTaskStateChanged(1, ActorTaskState.ACTING);
        verify(mListener).onStateChanged(ButtonState.WORKING, false);

        // Trigger state change to NEEDS_REVIEW.
        when(task.getState()).thenReturn(ActorTaskState.WAITING_ON_USER);
        mController.onTaskStateChanged(1, ActorTaskState.WAITING_ON_USER);
        verify(mListener).onStateChanged(ButtonState.NEEDS_REVIEW, false);

        // Trigger state change to DONE.
        when(task.getState()).thenReturn(ActorTaskState.FINISHED);
        mController.onTaskStateChanged(1, ActorTaskState.FINISHED);
        verify(mListener).onStateChanged(ButtonState.DONE, false);
    }

    @Test
    public void testOnGlobalShowHide() {
        mController.updateObservations(mProfile);

        when(mTask.getNativeBrowserWindowPtr(mProfile, mActivity)).thenReturn(123L);
        when(mGlicKeyedService.isPanelShowingForBrowser(123L)).thenReturn(true);

        mController.onGlobalShowHide();
        verify(mListener).onStateChanged(ButtonState.DEFAULT, true);

        when(mGlicKeyedService.isPanelShowingForBrowser(123L)).thenReturn(false);
        mController.onGlobalShowHide();
        verify(mListener).onStateChanged(ButtonState.DEFAULT, false);
    }

    @Test
    public void testUpdateButtonState() {
        mController.updateObservations(mProfile);

        ActorTask task = mock(ActorTask.class);
        when(task.getState()).thenReturn(ActorTaskState.ACTING);
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        mController.updateButtonState();
        verify(mListener).onStateChanged(ButtonState.WORKING, false);
    }

    @Test
    public void testUpdateButtonState_MultipleTasks() {
        mController.updateObservations(mProfile);

        ActorTask task1 = mock(ActorTask.class);
        when(task1.getState()).thenReturn(ActorTaskState.FINISHED);

        ActorTask task2 = mock(ActorTask.class);
        when(task2.getState()).thenReturn(ActorTaskState.WAITING_ON_USER);

        when(mActorService.getActiveTasks()).thenReturn(Arrays.asList(task1, task2));

        mController.updateButtonState();
        verify(mListener).onStateChanged(ButtonState.NEEDS_REVIEW, false);
    }

    @Test
    public void testUpdateButtonState_NoChange() {
        mController.updateObservations(mProfile);

        mController.updateButtonState(); // Initial state is DEFAULT
        verify(mListener, never()).onStateChanged(any(Integer.class), any(Boolean.class));
    }
}
