// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Java-side representation of the C++ ActorKeyedService. Manages the lifecycle and observation of
 * ActorTasks. This class combines the interface and implementation.
 */
@JNINamespace("actor")
@NullMarked
public class ActorKeyedService {
    private long mNativePtr;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /** Observer interface for ActorKeyedService events. */
    public interface Observer {
        /** Triggered when a task switches states (e.g., from ACTING to PAUSED). */
        void onTaskStateChanged(@ActorTaskId int taskId, @ActorTaskState int newState);
    }

    @CalledByNative
    private static ActorKeyedService create(long nativePtr) {
        return new ActorKeyedService(nativePtr);
    }

    private ActorKeyedService(long nativePtr) {
        mNativePtr = nativePtr;
    }

    /**
     * Returns all tasks currently managed by the service by reading through the list. No caching on
     * Java side.
     */
    public List<ActorTask> getActiveTasks() {
        if (mNativePtr == 0) return new ArrayList<>();
        ActorTask[] tasks = ActorKeyedServiceJni.get().getActiveTasks(mNativePtr);
        List<ActorTask> taskList = new ArrayList<>();
        if (tasks != null) {
            Collections.addAll(taskList, tasks);
        }
        return taskList;
    }

    /** Returns the number of active tasks. */
    public int getActiveTasksCount() {
        if (mNativePtr == 0) return 0;
        return ActorKeyedServiceJni.get().getActiveTasksCount(mNativePtr);
    }

    /** Gets a specific task by its ID. */
    @Nullable
    public ActorTask getTask(@ActorTaskId int taskId) {
        if (mNativePtr == 0) return null;
        return ActorKeyedServiceJni.get().getTask(mNativePtr, taskId);
    }

    /** Allows the UI to stop a running task. */
    public void stopTask(@ActorTaskId int taskId, @StoppedReason int stopReason) {
        if (mNativePtr == 0) return;
        ActorKeyedServiceJni.get().stopTask(mNativePtr, taskId, stopReason);
    }

    /** Allows Java UI components to listen for task creation, destruction, or state changes. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** Removes an observer. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @param tabId The tab ID to get a task on.
     * @return the task ID that is currently acting on the given tab, or null if none.
     */
    public @Nullable @ActorTaskId Integer getActiveTaskIdOnTab(int tabId) {
        if (mNativePtr == 0) return null;
        List<ActorTask> tasks = getActiveTasks();
        for (ActorTask task : tasks) {
            if (task.isActingOnTab(tabId)) {
                return task.getId();
            }
        }
        return null;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
    }

    @CalledByNative
    private void onTaskStateChanged(@ActorTaskId int taskId, @ActorTaskState int newState) {
        for (Observer obs : mObservers) {
            obs.onTaskStateChanged(taskId, newState);
        }
    }

    @NativeMethods
    interface Natives {
        ActorTask[] getActiveTasks(long nativeActorKeyedServiceAndroid);

        int getActiveTasksCount(long nativeActorKeyedServiceAndroid);

        ActorTask getTask(long nativeActorKeyedServiceAndroid, int taskId);

        void stopTask(long nativeActorKeyedServiceAndroid, int taskId, int stopReason);
    }
}
