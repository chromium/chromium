// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
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
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.Arrays;
import java.util.Collections;

/** Unit tests for {@link ActorTaskHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorTaskHelperTest {
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorService;
    @Mock private ActorTask mActorTask;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    private Activity mActivity;
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;
    private SettableMonotonicObservableSupplier<TabModelSelector> mSelectorSupplier;
    private ActorTaskHelper mActorTaskHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).create().get();

        mProfileSupplier = ObservableSuppliers.createMonotonic();
        mProfileSupplier.set(mProfile);
        mSelectorSupplier = ObservableSuppliers.createMonotonic();
        mSelectorSupplier.set(mTabModelSelector);

        when(mTabModelSelector.getTabById(1)).thenReturn(mTab);
        when(mActorTask.getTabs()).thenReturn(Collections.singleton(1));
        ActorKeyedServiceFactory.setForTesting(mActorService);

        mActorTaskHelper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        mSelectorSupplier,
                        mActivityLifecycleDispatcher);
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
    public void testOnStop() {
        ActorTask taskCreated = mock(ActorTask.class);
        when(taskCreated.getState()).thenReturn(ActorTaskState.CREATED);
        when(taskCreated.getTabs()).thenReturn(Collections.singleton(1));

        ActorTask taskActing = mock(ActorTask.class);
        when(taskActing.getState()).thenReturn(ActorTaskState.ACTING);
        when(taskActing.getTabs()).thenReturn(Collections.singleton(1));

        ActorTask taskReflecting = mock(ActorTask.class);
        when(taskReflecting.getState()).thenReturn(ActorTaskState.REFLECTING);
        when(taskReflecting.getTabs()).thenReturn(Collections.singleton(1));

        ActorTask taskPaused = mock(ActorTask.class);
        when(taskPaused.getState()).thenReturn(ActorTaskState.PAUSED_BY_USER);
        when(taskPaused.getTabs()).thenReturn(Collections.singleton(1));

        when(mActorService.getActiveTasks())
                .thenReturn(Arrays.asList(taskCreated, taskActing, taskReflecting, taskPaused));

        mActorTaskHelper.onStopWithNative();

        verify(taskCreated).pause();
        verify(taskActing).pause();
        verify(taskReflecting).pause();
        verify(taskPaused, never()).pause();
    }

    @Test
    public void testDestroy() {
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);
        mActorTaskHelper.onTaskStateChanged(1, ActorTaskState.ACTING);

        mActorTaskHelper.destroy();

        verify(mActivityLifecycleDispatcher).unregister(mActorTaskHelper);
        verify(mActorService, atLeastOnce()).removeObserver(mActorTaskHelper);
        assertFalse(
                (mActivity.getWindow().getAttributes().flags
                                & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        != 0);
    }

    @Test
    public void testOnStop_OnlyCurrentWindow() {
        TabModelSelector selector = mock(TabModelSelector.class);
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(selector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        ActorTask taskInWindow = mock(ActorTask.class);
        when(taskInWindow.getState()).thenReturn(ActorTaskState.ACTING);
        when(taskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        Tab tab101 = mock(Tab.class);
        when(selector.getTabById(101)).thenReturn(tab101);

        ActorTask taskOtherWindow = mock(ActorTask.class);
        when(taskOtherWindow.getState()).thenReturn(ActorTaskState.ACTING);
        when(taskOtherWindow.getTabs()).thenReturn(Collections.singleton(102));
        when(selector.getTabById(102)).thenReturn(null);

        when(mActorService.getActiveTasks())
                .thenReturn(Arrays.asList(taskInWindow, taskOtherWindow));

        helper.onStopWithNative();

        verify(taskInWindow).pause();
        verify(taskOtherWindow, never()).pause();
    }
}
