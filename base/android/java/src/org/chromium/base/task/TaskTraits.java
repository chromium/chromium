// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import java.util.Arrays;

import javax.annotation.Nullable;

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

    public TaskTraits() {}

    public TaskTraits setTaskPriority(int taskPriority) {
        mPrioritySetExplicitly = true;
        mPriority = taskPriority;
        return this;
    }

    /**
     * Tasks with this trait may block. This includes but is not limited to tasks that wait on
     * synchronous file I/O operations: read or write a file from disk, interact with a pipe or a
     * socket, rename or delete a file, enumerate files in a directory, etc. This trait isn't
     * required for the mere use of locks.
     */
    public TaskTraits setMayBlock(boolean mayBlock) {
        mMayBlock = mayBlock;
        return this;
    }

    // For convenience of the JNI code, we use primitive types only.
    // Note shutdown behavior is not supported on android.
    boolean mPrioritySetExplicitly;
    int mPriority = TaskPriority.USER_VISIBLE;
    boolean mMayBlock;
    byte mExtensionId = INVALID_EXTENSION_ID;
    byte mExtensionData[];

    protected void setExtensionId(byte extensionId) {
        mExtensionId = extensionId;
    }

    protected void setExtensionData(byte[] extensionData) {
        mExtensionData = extensionData;
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (object == this) {
            return true;
        } else if (object instanceof TaskTraits) {
            TaskTraits other = (TaskTraits) object;
            return mPrioritySetExplicitly == other.mPrioritySetExplicitly
                    && mPriority == other.mPriority && mExtensionId == other.mExtensionId
                    && Arrays.equals(mExtensionData, other.mExtensionData);
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
        hash = 37 * hash + (int) mExtensionId;
        hash = 37 * hash + Arrays.hashCode(mExtensionData);
        return hash;
    }
}
