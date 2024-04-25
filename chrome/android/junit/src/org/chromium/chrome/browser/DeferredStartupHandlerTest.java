// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Looper;
import android.os.MessageQueue.IdleHandler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadow.api.Shadow;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for DeferredStartupHandler. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowIdleHandlerAwareMessageQueue.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class DeferredStartupHandlerTest {
    private DeferredStartupHandler mDeferredStartupHandler;
    private ShadowIdleHandlerAwareMessageQueue mShadowMessageQueue;

    @Before
    public void setUp() {
        mShadowMessageQueue = (ShadowIdleHandlerAwareMessageQueue) Shadow.extract(Looper.myQueue());
        mShadowMessageQueue.clearIdleHandlers();
        mDeferredStartupHandler = new DeferredStartupHandler();
    }

    @Test
    public void addDeferredTask_SingleTask() {
        CallbackHelper helper = new CallbackHelper();
        mDeferredStartupHandler.addDeferredTask(() -> helper.notifyCalled());

        Assert.assertEquals(0, helper.getCallCount());
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(0, helper.getCallCount());
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());

        mShadowMessageQueue.runIdleHandlers();
        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());
    }

    @Test
    public void addDeferredTask_MultipleSingleTasks() {
        CallbackHelper helper = new CallbackHelper();
        mDeferredStartupHandler.addDeferredTask(() -> helper.notifyCalled());
        mDeferredStartupHandler.addDeferredTask(() -> helper.notifyCalled());
        mDeferredStartupHandler.addDeferredTask(() -> helper.notifyCalled());

        Assert.assertEquals(0, helper.getCallCount());
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());

        mShadowMessageQueue.runIdleHandlers();
        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());

        mShadowMessageQueue.runIdleHandlers();
        Assert.assertEquals(2, helper.getCallCount());
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());

        mShadowMessageQueue.runIdleHandlers();
        Assert.assertEquals(3, helper.getCallCount());
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());
    }

    @Test
    public void addDeferredTask_MultipleTasks() {
        CallbackHelper helper = new CallbackHelper();
        List<Runnable> tasks = new ArrayList<>();
        tasks.add(() -> helper.notifyCalled());
        tasks.add(() -> helper.notifyCalled());
        tasks.add(() -> helper.notifyCalled());
        mDeferredStartupHandler.addDeferredTasks(tasks);

        Assert.assertEquals(0, helper.getCallCount());
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());

        mShadowMessageQueue.runIdleHandlers();
        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());

        mShadowMessageQueue.runIdleHandlers();
        Assert.assertEquals(2, helper.getCallCount());
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());

        mShadowMessageQueue.runIdleHandlers();
        Assert.assertEquals(3, helper.getCallCount());
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());
    }

    @Test
    public void addDeferredTask_WhileIdleHandlerRunning() {
        CallbackHelper helper = new CallbackHelper();
        mDeferredStartupHandler.addDeferredTask(
                () -> {
                    helper.notifyCalled();
                    // Add a new deferred task.
                    mDeferredStartupHandler.addDeferredTask(() -> helper.notifyCalled());
                });

        Assert.assertEquals(0, helper.getCallCount());
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());

        mShadowMessageQueue.runIdleHandlers();
        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());

        // The subsequent IdleHandler pass should run the newly added task.
        mShadowMessageQueue.runIdleHandlers();
        Assert.assertEquals(2, helper.getCallCount());
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());
    }

    @Test
    public void addDeferredTask_AfterIdleHandlerRan() {
        CallbackHelper helper = new CallbackHelper();
        mDeferredStartupHandler.addDeferredTask(() -> helper.notifyCalled());

        Assert.assertEquals(0, helper.getCallCount());
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());

        mShadowMessageQueue.runIdleHandlers();

        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());

        // Add a new task.
        CallbackHelper helper2 = new CallbackHelper();
        mDeferredStartupHandler.addDeferredTask(() -> helper2.notifyCalled());
        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(0, helper2.getCallCount());
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());

        // Ensure a new request to queue can process these tasks.
        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());

        mShadowMessageQueue.runIdleHandlers();

        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(1, helper2.getCallCount());
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());
    }

    @Test
    public void queueDeferredTasksOnIdleHandler_MultipleActivities() {
        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());
        IdleHandler initialIdleHandler = mShadowMessageQueue.getIdleHandlers().get(0);

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertTrue(mShadowMessageQueue.getIdleHandlers().size() >= 1);
        Assert.assertEquals(initialIdleHandler, mShadowMessageQueue.getIdleHandlers().get(0));

        mShadowMessageQueue.runIdleHandlers();

        Assert.assertEquals(0, mShadowMessageQueue.getIdleHandlers().size());

        // Ensure a call queueDeferredTasksOnIdleHandler after the previous IdleHandler completes
        // adds a new IdleHandler.
        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mShadowMessageQueue.getIdleHandlers().size());
    }
}
