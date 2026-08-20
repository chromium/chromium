// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

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

        /** Triggered when a task's intermediate step progress (worklog) is updated. */
        default void onTaskStepProgressUpdated(@ActorTaskId int taskId, String stepProgress) {}
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
        // Fast-path early out to avoid JNI array allocation overhead if there are no tasks,
        // effectively suppressing GC pressure for idle clients.
        if (mNativePtr == 0 || getActiveTasksCount() == 0) return Collections.emptyList();
        return ActorKeyedServiceJni.get().getActiveTasks(mNativePtr);
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

    /** Returns the current active task if existing, null otherwise. */
    public @Nullable ActorTask getCurrentActiveTask() {
        if (mNativePtr == 0) return null;
        List<ActorTask> tasks = getActiveTasks();
        return !tasks.isEmpty() ? tasks.get(0) : null;
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
        return getActiveTaskIdOnTab(tabId, /* includePaused= */ false);
    }

    /**
     * @param tabId The tab ID to get a task on.
     * @param includePaused Whether to include tasks that are paused.
     * @return the task ID that is currently acting on the given tab, or null if none.
     */
    public @Nullable @ActorTaskId Integer getActiveTaskIdOnTab(int tabId, boolean includePaused) {
        if (mNativePtr == 0) return null;
        List<ActorTask> tasks = getActiveTasks();
        for (ActorTask task : tasks) {
            if (includePaused ? task.getTabs().contains(tabId) : task.isActingOnTab(tabId)) {
                return task.getId();
            }
        }
        return null;
    }

    @CalledByNative
    private void ensureForegroundServiceStarted(
            @JniType("std::string") String glicTriggerMessageId) {
        ActorForegroundServiceController.get().startService(glicTriggerMessageId);
    }

    /**
     * Called when a background tab is ready for actuation.
     *
     * @param tab The prepared tab.
     * @param glicTriggerMessageId The GLIC trigger message ID associated with the request.
     */
    public void setPreparedBackgroundTab(Tab tab, String glicTriggerMessageId) {
        if (mNativePtr == 0) return;
        ActorKeyedServiceJni.get().setPreparedBackgroundTab(mNativePtr, tab, glicTriggerMessageId);
    }

    /**
     * Called when background setup fails.
     *
     * @param glicTriggerMessageId The GLIC trigger message ID associated with the request.
     */
    public void notifyBackgroundSetupFailed(String glicTriggerMessageId) {
        if (mNativePtr == 0) return;
        ActorKeyedServiceJni.get().notifyBackgroundSetupFailed(mNativePtr, glicTriggerMessageId);
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

    @CalledByNative
    private void onTaskStepProgressChanged(@ActorTaskId int taskId, String stepProgress) {
        for (Observer obs : mObservers) {
            obs.onTaskStepProgressUpdated(taskId, stepProgress);
        }
    }

    @NativeMethods
    interface Natives {
        @JniType("std::vector<jni_zero::ScopedJavaLocalRef<jobject>>")
        List<ActorTask> getActiveTasks(long nativeActorKeyedServiceAndroid);

        int getActiveTasksCount(long nativeActorKeyedServiceAndroid);

        ActorTask getTask(long nativeActorKeyedServiceAndroid, int taskId);

        void stopTask(long nativeActorKeyedServiceAndroid, int taskId, int stopReason);

        void setPreparedBackgroundTab(
                long nativeActorKeyedServiceAndroid,
                @JniType("TabAndroid*") Tab tab,
                @JniType("std::string") String glicTriggerMessageId);

        void notifyBackgroundSetupFailed(
                long nativeActorKeyedServiceAndroid,
                @JniType("std::string") String glicTriggerMessageId);
    }

    @CalledByNative
    private void createBackgroundTabForTask(
            @JniType("Profile*") Profile profile,
            @ActorTaskId int taskId,
            Callback<@Nullable Tab> callback) {
        ActorForegroundServiceController.get()
                .provisionBackgroundTabForTask(profile, taskId, callback);
    }
}
