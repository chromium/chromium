// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager;

import org.jni_zero.NativeMethods;

/**
 * This class acts as a bridge between Java and the C++ task manager, providing the methods for
 * receiving updates and fetching specific task information (e.g., memory usage) from the C++ task
 * manager logic.
 */
class TaskManagerServiceBridge {
    /**
     * Adds the observer.
     *
     * @param refreshTimeMillis Specifies how often the observer should be notified. See
     *     TaskManagerObserver in task_manager_observer.h for specifics.
     * @param refreshType Specifies the types of resources that the observer should receive. See
     *     RefreshType in task_manager_observer.h for details.
     * @return The handle of the observer, which should be used on calling removeObserver.
     */
    ObserverHandle addObserver(
            TaskManagerObserver observer, int refreshTimeMillis, @RefreshType int refreshType) {
        long pointer =
                TaskManagerServiceBridgeJni.get()
                        .addObserver(observer, refreshTimeMillis, refreshType);
        return new ObserverHandle(pointer);
    }

    /** Removes the observer. */
    void removeObserver(ObserverHandle handle) {
        TaskManagerServiceBridgeJni.get().removeObserver(handle.getPointer());
    }

    // Following methods for fetching task information mirror the functions in
    // task_manager_interface.h. See the file for details.

    String getTitle(long taskId) {
        return TaskManagerServiceBridgeJni.get().getTitle(taskId);
    }

    long getMemoryFootprintUsage(long taskId) {
        return TaskManagerServiceBridgeJni.get().getMemoryFootprintUsage(taskId);
    }

    double getPlatformIndependentCpuUsage(long taskId) {
        return TaskManagerServiceBridgeJni.get().getPlatformIndependentCpuUsage(taskId);
    }

    long getProcessId(long taskId) {
        return TaskManagerServiceBridgeJni.get().getProcessId(taskId);
    }

    static class ObserverHandle {
        private long mPointer;

        ObserverHandle(long pointer) {
            mPointer = pointer;
        }

        long getPointer() {
            return mPointer;
        }
    }

    @NativeMethods
    interface Natives {
        long addObserver(
                TaskManagerObserver observer, int refreshTimeMillis, @RefreshType int refreshType);

        void removeObserver(long pointer);

        String getTitle(long taskid);

        long getMemoryFootprintUsage(long taskId);

        double getPlatformIndependentCpuUsage(long taskId);

        long getProcessId(long taskId);
    }
}
