// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager;

import org.jni_zero.CalledByNative;

/**
 * This interface defines the methods that an observer must implement to receive updates from the
 * C++ task manager class via JNI.
 *
 * <p>An instance of a class implementing this interface can be passed to the C++ side and
 * registered with the task manager using TaskManagerServiceBridge. The C++ task manager will then
 * call the methods defined in this interface to notify the Java observer about events related to
 * tasks.
 *
 * <p>This interface mirrors the functionality of the C++ `task_manager_observer.h` header file.
 */
public interface TaskManagerObserver {
    @CalledByNative
    void onTaskAdded(long id);

    @CalledByNative
    void onTaskToBeRemoved(long id);

    @CalledByNative
    void onTasksRefreshed(long[] taskIds);

    @CalledByNative
    void onTasksRefreshedWithBackgroundCalculations(long[] taskIds);

    @CalledByNative
    void onTaskUnresponsive(long id);
}
