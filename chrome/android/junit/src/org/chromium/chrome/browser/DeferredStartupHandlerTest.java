// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.MessageQueue;
import android.os.MessageQueue.IdleHandler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for DeferredStartupHandler. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class DeferredStartupHandlerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private final List<IdleHandler> mIdleHandlers = new ArrayList<>();

    @Mock private MessageQueue mMessageQueue;

    private DeferredStartupHandler mDeferredStartupHandler;

    @Before
    public void setUp() {
        mDeferredStartupHandler = new DeferredStartupHandler(mMessageQueue);
        Mockito.doAnswer(
                        invocation -> {
                            mIdleHandlers.add((IdleHandler) invocation.getArguments()[0]);
                            return null;
                        })
                .when(mMessageQueue)
                .addIdleHandler(Mockito.any(IdleHandler.class));
    }

    @Test
    public void addDeferredTask_SingleTask() {
        CallbackHelper helper = new CallbackHelper();
        mDeferredStartupHandler.addDeferredTask(() -> helper.notifyCalled());

        Assert.assertEquals(0, helper.getCallCount());
        Assert.assertEquals(0, mIdleHandlers.size());

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(0, helper.getCallCount());
        Assert.assertEquals(1, mIdleHandlers.size());

        runIdleHandlers();
        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(0, mIdleHandlers.size());
    }

    @Test
    public void addDeferredTask_MultipleSingleTasks() {
        CallbackHelper helper = new CallbackHelper();
        mDeferredStartupHandler.addDeferredTask(() -> helper.notifyCalled());
        mDeferredStartupHandler.addDeferredTask(() -> helper.notifyCalled());
        mDeferredStartupHandler.addDeferredTask(() -> helper.notifyCalled());

        Assert.assertEquals(0, helper.getCallCount());
        Assert.assertEquals(0, mIdleHandlers.size());

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mIdleHandlers.size());

        runIdleHandlers();
        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(1, mIdleHandlers.size());

        runIdleHandlers();
        Assert.assertEquals(2, helper.getCallCount());
        Assert.assertEquals(1, mIdleHandlers.size());

        runIdleHandlers();
        Assert.assertEquals(3, helper.getCallCount());
        Assert.assertEquals(0, mIdleHandlers.size());
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
        Assert.assertEquals(0, mIdleHandlers.size());

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mIdleHandlers.size());

        runIdleHandlers();
        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(1, mIdleHandlers.size());

        runIdleHandlers();
        Assert.assertEquals(2, helper.getCallCount());
        Assert.assertEquals(1, mIdleHandlers.size());

        runIdleHandlers();
        Assert.assertEquals(3, helper.getCallCount());
        Assert.assertEquals(0, mIdleHandlers.size());
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
        Assert.assertEquals(0, mIdleHandlers.size());

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mIdleHandlers.size());

        runIdleHandlers();
        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(1, mIdleHandlers.size());

        // The subsequent IdleHandler pass should run the newly added task.
        runIdleHandlers();
        Assert.assertEquals(2, helper.getCallCount());
        Assert.assertEquals(0, mIdleHandlers.size());
    }

    @Test
    public void addDeferredTask_AfterIdleHandlerRan() {
        CallbackHelper helper = new CallbackHelper();
        mDeferredStartupHandler.addDeferredTask(() -> helper.notifyCalled());

        Assert.assertEquals(0, helper.getCallCount());
        Assert.assertEquals(0, mIdleHandlers.size());

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mIdleHandlers.size());

        runIdleHandlers();

        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(0, mIdleHandlers.size());

        // Add a new task.
        CallbackHelper helper2 = new CallbackHelper();
        mDeferredStartupHandler.addDeferredTask(() -> helper2.notifyCalled());
        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(0, helper2.getCallCount());
        Assert.assertEquals(0, mIdleHandlers.size());

        // Ensure a new request to queue can process these tasks.
        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mIdleHandlers.size());

        runIdleHandlers();

        Assert.assertEquals(1, helper.getCallCount());
        Assert.assertEquals(1, helper2.getCallCount());
        Assert.assertEquals(0, mIdleHandlers.size());
    }

    @Test
    public void queueDeferredTasksOnIdleHandler_MultipleActivities() {
        Assert.assertEquals(0, mIdleHandlers.size());

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mIdleHandlers.size());
        IdleHandler initialIdleHandler = mIdleHandlers.get(0);

        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertTrue(mIdleHandlers.size() >= 1);
        Assert.assertEquals(initialIdleHandler, mIdleHandlers.get(0));

        runIdleHandlers();

        Assert.assertEquals(0, mIdleHandlers.size());

        // Ensure a call queueDeferredTasksOnIdleHandler after the previous IdleHandler completes
        // adds a new IdleHandler.
        mDeferredStartupHandler.queueDeferredTasksOnIdleHandler();
        Assert.assertEquals(1, mIdleHandlers.size());
    }

    private void runIdleHandlers() {
        List<IdleHandler> idleHandlers;
        synchronized (mIdleHandlers) {
            idleHandlers = new ArrayList<>(mIdleHandlers);
        }
        for (int i = 0; i < idleHandlers.size(); i++) {
            IdleHandler handler = idleHandlers.get(i);
            if (!handler.queueIdle()) mIdleHandlers.remove(handler);
        }
    }
}
