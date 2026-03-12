// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.widget.FrameLayout;

import androidx.activity.ComponentActivity;
import androidx.core.pip.PictureInPictureDelegate;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.actor.ui.ActorPictureInPictureOverlayCoordinator;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.function.Supplier;

/** Unit tests for {@link ActorPictureInPictureController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorPictureInPictureControllerTest {
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorService;
    @Mock private ActorKeyedServiceFactory.Natives mActorKeyedServiceFactoryJni;
    @Mock private ActorPictureInPictureOverlayCoordinator mMockCoordinator;

    private ComponentActivity mActivity;
    private Supplier<Profile> mProfileSupplier;
    private ActorPictureInPictureController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ActorKeyedServiceFactoryJni.setInstanceForTesting(mActorKeyedServiceFactoryJni);

        mActivity = Robolectric.buildActivity(ComponentActivity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        FrameLayout contentView = new FrameLayout(mActivity);
        contentView.setId(android.R.id.content);
        mActivity.setContentView(contentView);

        mProfileSupplier = () -> mProfile;

        mController =
                new ActorPictureInPictureController(
                        mActivity,
                        mProfileSupplier,
                        () -> mActivity.findViewById(android.R.id.content));
        mController.setOverlayCoordinatorForTesting(mMockCoordinator);
        ActorKeyedServiceFactory.setForTesting(mActorService);
    }

    @Test
    public void testShouldEnterPip_NoService() {
        ActorKeyedServiceFactory.setForTesting(null);
        assertFalse(mController.shouldEnterPip());
    }

    @Test
    public void testShouldEnterPip_NoActiveTasks() {
        when(mActorService.getActiveTasksCount()).thenReturn(0);
        assertFalse(mController.shouldEnterPip());
    }

    @Test
    public void testShouldEnterPip_ActiveTasks() {
        when(mActorService.getActiveTasksCount()).thenReturn(1);

        assertTrue(mController.shouldEnterPip());
    }

    @Test
    public void testOnPictureInPictureEvent_Entered_ShowsOverlay() {
        when(mActorService.getActiveTasksCount()).thenReturn(1);
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.getTitle()).thenReturn("Test Title");
        when(mockTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mActorService.getCurrentActiveTask()).thenReturn(mockTask);

        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);

        verify(mMockCoordinator).setVisibility(true);
        verify(mMockCoordinator).updateTitle("Test Title");
        verify(mMockCoordinator).updateStatus(ActorTaskState.ACTING);
        verify(mMockCoordinator, never()).destroy();
    }

    @Test
    public void testOnPictureInPictureEvent_Exited_HidesOverlay() {
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.EXITED, null);
        verify(mMockCoordinator).setVisibility(false);
    }

    @Test
    public void testOnTaskStateChanged_UpdatesOverlayWhenInPip() {
        when(mActorService.getActiveTasksCount()).thenReturn(1);
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.getTitle()).thenReturn("Shopping Task");
        when(mockTask.getState()).thenReturn(ActorTaskState.CREATED);
        when(mActorService.getCurrentActiveTask()).thenReturn(mockTask);

        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);

        verify(mMockCoordinator).updateTitle("Shopping Task");

        // Change state
        int newState = ActorTaskState.ACTING;
        mController.onTaskStateChanged(123, newState);

        verify(mMockCoordinator).updateStatus(newState);
        verify(mMockCoordinator, times(1)).updateTitle(any());
    }

    @Test
    public void testAttemptPictureInPicture_Success() {
        when(mActorService.getActiveTasksCount()).thenReturn(1);

        ComponentActivity spyActivity = spy(mActivity);
        doNothing().when(spyActivity).enterPictureInPictureMode();

        mController =
                new ActorPictureInPictureController(
                        spyActivity,
                        mProfileSupplier,
                        () -> mActivity.findViewById(android.R.id.content));

        mController.attemptPictureInPicture();

        verify(spyActivity).enterPictureInPictureMode();
    }

    @Test
    public void testAttemptPictureInPicture_NoTasks() {
        when(mActorService.getActiveTasksCount()).thenReturn(0);
        ComponentActivity spyActivity = spy(mActivity);

        mController =
                new ActorPictureInPictureController(
                        spyActivity,
                        mProfileSupplier,
                        () -> mActivity.findViewById(android.R.id.content));

        mController.attemptPictureInPicture();

        verify(spyActivity, never()).enterPictureInPictureMode();
    }

    @Test
    public void testDestroy_RemovesObserver() {
        mController.shouldEnterPip();

        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);
        mController.destroy();

        verify(mMockCoordinator).destroy();
        verify(mActorService).removeObserver(mController);
    }

    @Test
    public void testOnTaskStateChanged_ExitsPipWhenNoTasks() {
        when(mActorService.getActiveTasksCount()).thenReturn(1);
        ComponentActivity spyActivity = spy(mActivity);
        // Use spyActivity in controller
        mController =
                new ActorPictureInPictureController(
                        spyActivity,
                        mProfileSupplier,
                        () -> mActivity.findViewById(android.R.id.content));

        mController.setOverlayCoordinatorForTesting(mMockCoordinator);

        // Enter PiP
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);

        // Task finishes
        when(mActorService.getActiveTasksCount()).thenReturn(0);
        mController.onTaskStateChanged(1, 0); // 0 as dummy state

        verify(spyActivity).moveTaskToBack(true);
        verify(mMockCoordinator).setVisibility(false);
    }
}
