// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.WindowManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Collections;

/** Unit tests for {@link ActorTaskHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorTaskHelperTest {
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorService;
    @Mock private ActorTask mActorTask;

    private Activity mActivity;
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;
    private ActorTaskHelper mActorTaskHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).create().get();

        mProfileSupplier = ObservableSuppliers.createMonotonic();
        mProfileSupplier.set(mProfile);
        ActorKeyedServiceFactory.setForTesting(mActorService);

        mActorTaskHelper = new ActorTaskHelper(mActivity, mProfileSupplier);
    }

    @Test
    public void testKeepScreenOn_TaskActive() {
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);

        mActorTaskHelper.onTaskStateChanged(1, ActorTaskState.ACTING);

        assertTrue(
                (mActivity.getWindow().getAttributes().flags
                                & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        != 0);
        verify(mActorService).addObserver(mActorTaskHelper);
    }

    @Test
    public void testKeepScreenOn_TaskInactive() {
        // Start with active task
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);
        mActorTaskHelper.onTaskStateChanged(1, ActorTaskState.ACTING);
        assertTrue(
                (mActivity.getWindow().getAttributes().flags
                                & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        != 0);

        // Task finished
        when(mActorTask.getState()).thenReturn(ActorTaskState.FINISHED);
        mActorTaskHelper.onTaskStateChanged(1, ActorTaskState.FINISHED);

        assertFalse(
                (mActivity.getWindow().getAttributes().flags
                                & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        != 0);
    }

    @Test
    public void testDestroy() {
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);
        mActorTaskHelper.onTaskStateChanged(1, ActorTaskState.ACTING);

        mActorTaskHelper.destroy();

        verify(mActorService, atLeastOnce()).removeObserver(mActorTaskHelper);
        assertFalse(
                (mActivity.getWindow().getAttributes().flags
                                & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        != 0);
    }
}
