// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import androidx.annotation.Nullable;

import java.util.Arrays;

/**
 * TaskTraits are metadata that influence how the TaskSecheduler deals with that task.
 * E.g. the trait can directly or indirectly control task prioritization.
 */
public class TaskTraits {
    // Keep in sync with base::TaskTraitsExtensionStorage:: kInvalidExtensionId
    public static final byte INVALID_EXTENSION_ID = 0;

    // Keep in sync with base::TaskTraitsExtensionStorage::kMaxExtensionId
    public static final int MAX_EXTENSION_ID = 4;

    // Keep in sync with base::TaskTraitsExtensionStorage::kStorageSize
    public static final int EXTENSION_STORAGE_SIZE = 8;

    // Convenience variables explicitly specifying common priorities

    // This task will only be scheduled when machine resources are available. Once
    // running, it may be descheduled if higher priority work arrives (in this
    // process or another) and its running on a non-critical thread. This is the
    // lowest possible priority.
    public static final TaskTraits BEST_EFFORT =
            new TaskTraits().taskPriority(TaskPriority.BEST_EFFORT);

    // This is a lowest-priority task which may block, for example non-urgent
    // logging or deletion of temporary files as clean-up.
    public static final TaskTraits BEST_EFFORT_MAY_BLOCK = BEST_EFFORT.mayBlock();

    // This task affects UI or responsiveness of future user interactions. It is
    // not an immediate response to a user interaction. Most tasks are likely to
    // have this priority.
    // Examples:
    // - Updating the UI to reflect progress on a long task.
    // - Loading data that might be shown in the UI after a future user
    //   interaction.
    public static final TaskTraits USER_VISIBLE =
            new TaskTraits().taskPriority(TaskPriority.USER_VISIBLE);

    // USER_VISIBLE + may block.
    public static final TaskTraits USER_VISIBLE_MAY_BLOCK = USER_VISIBLE.mayBlock();

    // This task affects UI immediately after a user interaction.
    // Example: Generating data shown in the UI immediately after a click.
    public static final TaskTraits USER_BLOCKING =
            new TaskTraits().taskPriority(TaskPriority.USER_BLOCKING);

    // USER_BLOCKING + may block.
    public static final TaskTraits USER_BLOCKING_MAY_BLOCK = USER_BLOCKING.mayBlock();

    // A bit like requestAnimationFrame, this task will be posted onto the Choreographer
    // and will be run on the android main thread after the next vsync.
    public static final TaskTraits CHOREOGRAPHER_FRAME = new TaskTraits();
    static {
        CHOREOGRAPHER_FRAME.mIsChoreographerFrame = true;
    }

    // For tasks that should run on the thread pool instead of the main thread.
    // Note that currently also tasks which lack this trait will execute on the
    // thread pool unless a trait for a named thread is given.
    public static final TaskTraits THREAD_POOL =
            new TaskTraits().threadPool().taskPriority(TaskPriority.USER_BLOCKING);
    public static final TaskTraits THREAD_POOL_USER_BLOCKING =
            THREAD_POOL.taskPriority(TaskPriority.USER_BLOCKING);
    public static final TaskTraits THREAD_POOL_USER_VISIBLE =
            THREAD_POOL.taskPriority(TaskPriority.USER_VISIBLE);
    public static final TaskTraits THREAD_POOL_BEST_EFFORT =
            THREAD_POOL.taskPriority(TaskPriority.BEST_EFFORT);

    // For tasks that should run on the current thread.
    public static final TaskTraits CURRENT_THREAD =
            new TaskTraits().currentThread().taskPriority(TaskPriority.USER_BLOCKING);
    public static final TaskTraits CURRENT_THREAD_USER_BLOCKING =
            CURRENT_THREAD.taskPriority(TaskPriority.USER_BLOCKING);
    public static final TaskTraits CURRENT_THREAD_USER_VISIBLE =
            CURRENT_THREAD.taskPriority(TaskPriority.USER_VISIBLE);
    public static final TaskTraits CURRENT_THREAD_BEST_EFFORT =
            CURRENT_THREAD.taskPriority(TaskPriority.BEST_EFFORT);

    // For convenience of the JNI code, we use primitive types only.
    // Note shutdown behavior is not supported on android.
    boolean mPrioritySetExplicitly;
    int mPriority;
    boolean mMayBlock;
    boolean mUseThreadPool;
    boolean mUseCurrentThread;
    byte mExtensionId;
    byte mExtensionData[];
    boolean mIsChoreographerFrame;

    // Derive custom traits from existing trait constants.
    private TaskTraits() {
        mPriority = TaskPriority.USER_VISIBLE;
    }

    private TaskTraits(TaskTraits other) {
        mPrioritySetExplicitly = other.mPrioritySetExplicitly;
        mPriority = other.mPriority;
        mMayBlock = other.mMayBlock;
        mUseThreadPool = other.mUseThreadPool;
        mExtensionId = other.mExtensionId;
        mUseCurrentThread = other.mUseCurrentThread;
        mExtensionData = other.mExtensionData;
    }

    public TaskTraits taskPriority(int taskPriority) {
        TaskTraits taskTraits = new TaskTraits(this);
        taskTraits.mPrioritySetExplicitly = true;
        taskTraits.mPriority = taskPriority;
        return taskTraits;
    }

    /**
     * Tasks with this trait may block. This includes but is not limited to tasks that wait on
     * synchronous file I/O operations: read or write a file from disk, interact with a pipe or a
     * socket, rename or delete a file, enumerate files in a directory, etc. This trait isn't
     * required for the mere use of locks. The thread pool uses this property to work out if
     * additional threads are required.
     */
    public TaskTraits mayBlock() {
        TaskTraits taskTraits = new TaskTraits(this);
        taskTraits.mMayBlock = true;
        return taskTraits;
    }

    public TaskTraits threadPool() {
        TaskTraits taskTraits = new TaskTraits(this);
        taskTraits.mUseThreadPool = true;
        return taskTraits;
    }

    public TaskTraits currentThread() {
        TaskTraits taskTraits = new TaskTraits(this);
        taskTraits.mUseCurrentThread = true;
        return taskTraits;
    }

    /**
     * @return true if this task is using some TaskTraits extension.
     */
    public boolean hasExtension() {
        return mExtensionId != INVALID_EXTENSION_ID;
    }

    /**
     * Tries to extract the extension for the given descriptor from this traits.
     *
     * @return Extension instance or null if the traits do not contain the requested extension
     */
    public <Extension> Extension getExtension(TaskTraitsExtensionDescriptor<Extension> descriptor) {
        if (mExtensionId == descriptor.getId()) {
            return descriptor.fromSerializedData(mExtensionData);
        } else {
            return null;
        }
    }

    public <Extension> TaskTraits withExtension(
            TaskTraitsExtensionDescriptor<Extension> descriptor, Extension extension) {
        int id = descriptor.getId();
        byte[] data = descriptor.toSerializedData(extension);
        assert id > INVALID_EXTENSION_ID && id <= MAX_EXTENSION_ID;
        assert data.length <= EXTENSION_STORAGE_SIZE;

        TaskTraits taskTraits = new TaskTraits(this);
        taskTraits.mExtensionId = (byte) id;
        taskTraits.mExtensionData = data;
        return taskTraits;
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (object == this) {
            return true;
        } else if (object instanceof TaskTraits) {
            TaskTraits other = (TaskTraits) object;
            return mPrioritySetExplicitly == other.mPrioritySetExplicitly
                    && mPriority == other.mPriority && mMayBlock == other.mMayBlock
                    && mUseThreadPool == other.mUseThreadPool
                    && mUseCurrentThread == other.mUseCurrentThread
                    && mExtensionId == other.mExtensionId
                    && Arrays.equals(mExtensionData, other.mExtensionData)
                    && mIsChoreographerFrame == other.mIsChoreographerFrame;
        } else {
            return false;
        }
    }

    @Override
    public int hashCode() {
        int hash = 31;
        hash = 37 * hash + (mPrioritySetExplicitly ? 0 : 1);
        hash = 37 * hash + mPriority;
        hash = 37 * hash + (mMayBlock ? 0 : 1);
        hash = 37 * hash + (mUseThreadPool ? 0 : 1);
        hash = 37 * hash + (mUseCurrentThread ? 0 : 1);
        hash = 37 * hash + (int) mExtensionId;
        hash = 37 * hash + Arrays.hashCode(mExtensionData);
        hash = 37 * hash + (mIsChoreographerFrame ? 0 : 1);
        return hash;
    }
}
