// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.profiles.Profile;

import java.util.function.Supplier;

/** Unit tests for {@link ActorPictureInPictureController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorPictureInPictureControllerTest {
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorService;
    @Mock private ActorKeyedServiceFactory.Natives mActorKeyedServiceFactoryJni;

    private ComponentActivity mActivity;
    private Supplier<Profile> mProfileSupplier;
    private ActorPictureInPictureController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ActorKeyedServiceFactoryJni.setInstanceForTesting(mActorKeyedServiceFactoryJni);

        mActivity = Robolectric.buildActivity(ComponentActivity.class).create().get();
        mProfileSupplier = () -> mProfile;

        ActorKeyedServiceFactory.setForTesting(mActorService);
    }

    @Test
    public void testShouldEnterPip_NoService() {
        ActorKeyedServiceFactory.setForTesting(null);
        mController = new ActorPictureInPictureController(mActivity, mProfileSupplier);
        assertFalse(mController.shouldEnterPip());
    }

    @Test
    public void testShouldEnterPip_NoActiveTasks() {
        when(mActorService.getActiveTasksCount()).thenReturn(0);
        mController = new ActorPictureInPictureController(mActivity, mProfileSupplier);
        assertFalse(mController.shouldEnterPip());
    }

    @Test
    public void testShouldEnterPip_ActiveTasks() {
        when(mActorService.getActiveTasksCount()).thenReturn(1);

        mController = new ActorPictureInPictureController(mActivity, mProfileSupplier);
        assertTrue(mController.shouldEnterPip());
    }

    @Test
    public void testAttemptPictureInPicture_Success() {
        when(mActorService.getActiveTasksCount()).thenReturn(1);

        ComponentActivity spyActivity = spy(mActivity);
        doNothing().when(spyActivity).enterPictureInPictureMode();

        mController = new ActorPictureInPictureController(spyActivity, mProfileSupplier);
        mController.attemptPictureInPicture();

        verify(spyActivity).enterPictureInPictureMode();
    }

    @Test
    public void testAttemptPictureInPicture_NoTasks() {
        when(mActorService.getActiveTasksCount()).thenReturn(0);
        ComponentActivity spyActivity = spy(mActivity);

        mController = new ActorPictureInPictureController(spyActivity, mProfileSupplier);
        mController.attemptPictureInPicture();

        verify(spyActivity, never()).enterPictureInPictureMode();
    }

    @Test
    public void testDestroy_RemovesObserver() {
        mController = new ActorPictureInPictureController(mActivity, mProfileSupplier);
        mController.shouldEnterPip();

        mController.destroy();
        verify(mActorService).removeObserver(mController);
    }

    @Test
    public void testOnTaskStateChanged_ExitsPipWhenNoTasks() {
        when(mActorService.getActiveTasksCount()).thenReturn(1);
        ComponentActivity spyActivity = spy(mActivity);
        // Use spyActivity in controller
        mController = new ActorPictureInPictureController(spyActivity, mProfileSupplier);

        // Enter PiP
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);

        // Task finishes
        when(mActorService.getActiveTasksCount()).thenReturn(0);
        mController.onTaskStateChanged(1, 0); // 0 as dummy state

        verify(spyActivity).moveTaskToBack(true);
    }
}
