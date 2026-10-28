// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.KeyEvent;

import org.jni_zero.JniUniquePtr;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.task_manager.TaskManagerServiceBridge;
import org.chromium.chrome.browser.task_manager.TaskManagerServiceBridgeJni;

@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TASK_MANAGER_TOOLBAR)
public class TaskManagerActivityTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TaskManagerServiceBridge.Natives mBridge;

    @Mock
    private JniUniquePtr<TaskManagerServiceBridge.NativeTaskManagerObserverAndroid> mNativeObserver;

    @Before
    public void setUp() {
        TaskManagerServiceBridgeJni.setInstanceForTesting(mBridge);
        when(mBridge.addObserver(any(), anyInt(), anyInt())).thenReturn(mNativeObserver);
    }

    @Test
    public void testLifecycle() {
        ActivityController<TaskManagerActivity> controller =
                Robolectric.buildActivity(TaskManagerActivity.class);
        Activity activity = controller.get();
        activity.setTheme(R.style.Theme_Chromium_Activity);

        controller.setup();

        verify(mBridge).addObserver(any(), anyInt(), anyInt());

        controller.destroy();

        verify(mNativeObserver).destroy();
    }

    @Test
    public void testCloseShortcuts_FinishesActivity() {
        KeyEvent[] events = {
            new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_W, 0, KeyEvent.META_CTRL_ON),
            new KeyEvent(
                    0,
                    0,
                    KeyEvent.ACTION_DOWN,
                    KeyEvent.KEYCODE_W,
                    0,
                    KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON),
            new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_F4, 0, KeyEvent.META_ALT_ON),
            new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE),
        };
        for (KeyEvent event : events) {
            TaskManagerActivity activity =
                    Robolectric.buildActivity(TaskManagerActivity.class).get();
            assertTrue(activity.onKeyDown(event.getKeyCode(), event));
            assertTrue(activity.isFinishing());
        }
    }

    @Test
    public void testCloseShortcuts_IgnoredOnRepeatOrUnsupportedModifiers() {
        TaskManagerActivity activity = Robolectric.buildActivity(TaskManagerActivity.class).get();
        KeyEvent[] events = {
            new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_W, 1, KeyEvent.META_CTRL_ON),
            new KeyEvent(
                    0,
                    0,
                    KeyEvent.ACTION_DOWN,
                    KeyEvent.KEYCODE_W,
                    0,
                    KeyEvent.META_META_ON | KeyEvent.META_CTRL_ON),
            new KeyEvent(
                    0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE, 0, KeyEvent.META_CTRL_ON),
        };
        for (KeyEvent event : events) {
            assertFalse(activity.onKeyDown(event.getKeyCode(), event));
            assertFalse(activity.isFinishing());
        }
    }
}
