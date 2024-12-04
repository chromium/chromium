// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.filters.SmallTest;

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
import org.chromium.chrome.browser.task_manager.TaskManagerServiceBridge;
import org.chromium.chrome.browser.task_manager.TaskManagerServiceBridgeJni;

@RunWith(BaseRobolectricTestRunner.class)
public class TaskManagerActivityTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TaskManagerServiceBridge.Natives mBridge;

    @Before
    public void setUp() {
        TaskManagerServiceBridgeJni.setInstanceForTesting(mBridge);
    }

    @Test
    @SmallTest
    public void testLifecycle() {
        ActivityController<TaskManagerActivity> controller =
                Robolectric.buildActivity(TaskManagerActivity.class);
        Activity activity = controller.get();
        activity.setTheme(R.style.Theme_Chromium_Activity);

        controller.setup();

        verify(mBridge).addObserver(any(), anyInt(), anyInt());

        controller.destroy();

        verify(mBridge).removeObserver(anyLong());
    }
}
