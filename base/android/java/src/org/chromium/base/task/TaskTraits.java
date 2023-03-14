// Copyright 2018 The Chromium Authors
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

    // Convenience variables explicitly specifying common priorities.
    // These also imply THREAD_POOL unless explicitly overwritten.
    // TODO(1026641): Make destination explicit in Java too.

    // This task will only be scheduled when machine resources are available. Once
    // running, it may be descheduled if higher priority work arrives (in this
    // process or another) and its running on a non-critical thread. This is the
    // lowest possible priority.
    public static final TaskTraits BEST_EFFORT = new TaskTraits(TaskPriority.BEST_EFFORT, false);

    // This is a lowest-priority task which may block, for example non-urgent
    // logging or deletion of temporary files as clean-up.
    public static final TaskTraits BEST_EFFORT_MAY_BLOCK =
            new TaskTraits(TaskPriority.BEST_EFFORT, true);

    // This task affects UI or responsiveness of future user interactions. It is
    // not an immediate response to a user interaction. Most tasks are likely to
    // have this priority.
    // Examples:
    // - Updating the UI to reflect progress on a long task.
    // - Loading data that might be shown in the UI after a future user
    //   interaction.
    public static final TaskTraits USER_VISIBLE = new TaskTraits(TaskPriority.USER_VISIBLE, false);

    // USER_VISIBLE + may block.
    public static final TaskTraits USER_VISIBLE_MAY_BLOCK =
            new TaskTraits(TaskPriority.USER_VISIBLE, true);

    // This task affects UI immediately after a user interaction.
    // Example: Generating data shown in the UI immediately after a click.
    public static final TaskTraits USER_BLOCKING =
            new TaskTraits(TaskPriority.USER_BLOCKING, false);

    // USER_BLOCKING + may block.
    public static final TaskTraits USER_BLOCKING_MAY_BLOCK =
            new TaskTraits(TaskPriority.USER_BLOCKING, true);

    // For convenience of the JNI code, we use primitive types only.
    // Note shutdown behavior is not supported on android.
    final int mPriority;
    final boolean mMayBlock;
    final boolean mUseThreadPool;
    final byte mExtensionId;
    final byte mExtensionData[];

    // For ThreadPool TaskTraits
    private TaskTraits(int priority, boolean mayBlock) {
        mPriority = priority;
        mMayBlock = mayBlock;
        mUseThreadPool = true;
        mExtensionId = INVALID_EXTENSION_ID;
        mExtensionData = null;
    }

    // For Extensions. MayBlock and ThreadPool are currently unused.
    private TaskTraits(int priority, byte extensionId, byte extensionData[]) {
        mPriority = priority;
        mMayBlock = false;
        mUseThreadPool = false;
        mExtensionId = extensionId;
        mExtensionData = extensionData;
    }

    public static <Extension> TaskTraits forExtension(int priority,
            TaskTraitsExtensionDescriptor<Extension> descriptor, Extension extension) {
        int id = descriptor.getId();
        byte[] data = descriptor.toSerializedData(extension);
        assert id > INVALID_EXTENSION_ID && id <= MAX_EXTENSION_ID;
        assert data.length <= EXTENSION_STORAGE_SIZE;

        return new TaskTraits(priority, (byte) id, data);
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

    @Override
    public boolean equals(@Nullable Object object) {
        if (object == this) {
            return true;
        } else if (object instanceof TaskTraits) {
            TaskTraits other = (TaskTraits) object;
            return mPriority == other.mPriority && mMayBlock == other.mMayBlock
                    && mUseThreadPool == other.mUseThreadPool && mExtensionId == other.mExtensionId
                    && Arrays.equals(mExtensionData, other.mExtensionData);
        } else {
            return false;
        }
    }

    @Override
    public int hashCode() {
        int hash = 31;
        hash = 37 * hash + mPriority;
        hash = 37 * hash + (mMayBlock ? 0 : 1);
        hash = 37 * hash + (mUseThreadPool ? 0 : 1);
        hash = 37 * hash + (int) mExtensionId;
        hash = 37 * hash + Arrays.hashCode(mExtensionData);
        return hash;
    }
}
