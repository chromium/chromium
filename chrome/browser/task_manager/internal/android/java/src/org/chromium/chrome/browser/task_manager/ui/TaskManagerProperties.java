// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.task_manager.TaskManagerServiceBridge.GpuMemoryUsage;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableLongPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableLongPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class contains the keys for retrieving the corresponding value from the model representing
 * the current task manager state.
 */
class TaskManagerProperties {
    @IntDef({
        RowType.TASK,
    })
    @Retention(RetentionPolicy.SOURCE)
    static @interface RowType {
        /** Represents a task. Each item with this key has prorperties of the corresponding task. */
        int TASK = 1;
    }

    /** Describes sort order of tasks. */
    static class SortDescriptor {
        public final PropertyKey key;
        public final boolean ascending;

        SortDescriptor(PropertyKey key, boolean ascending) {
            this.key = key;
            this.ascending = ascending;
        }

        @Override
        public boolean equals(Object other) {
            if (this == other) return true;
            if (other == null) return false;
            if (this.getClass() != other.getClass()) return false;
            SortDescriptor that = (SortDescriptor) other;
            return key == that.key && ascending == that.ascending;
        }
    }

    /** Property key for the columns of the header. */
    static final WritableObjectPropertyKey<PropertyKey[]> COLUMNS =
            new WritableObjectPropertyKey<>();

    /** Property key for the sort order. If the sort order is unspecified, null is set. */
    static final WritableObjectPropertyKey<SortDescriptor> SORT_DESCRIPTOR =
            new WritableObjectPropertyKey<>();

    /** All the property keys used by the header model. */
    static final PropertyKey[] HEADER_PROPERTY_KEYS = new PropertyKey[] {COLUMNS, SORT_DESCRIPTOR};

    /** Property key for task id. */
    static final ReadableLongPropertyKey TASK_ID = new ReadableLongPropertyKey();

    /** Property key for whether the task is selected. */
    static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();

    /** Property key for whether the task is killable. */
    static final ReadableBooleanPropertyKey IS_KILLABLE = new ReadableBooleanPropertyKey();

    /** Property key for task name. */
    static final WritableObjectPropertyKey<String> TASK_NAME = new WritableObjectPropertyKey<>();

    /** Property key for memory footprint. */
    static final WritableLongPropertyKey MEMORY_FOOTPRINT = new WritableLongPropertyKey();

    /** Property key for cpu. */
    static final WritableFloatPropertyKey CPU = new WritableFloatPropertyKey();

    /** Property key for network usage */
    static final WritableLongPropertyKey NETWORK_USAGE = new WritableLongPropertyKey();

    /** Property key for process id. */
    static final WritableLongPropertyKey PROCESS_ID = new WritableLongPropertyKey();

    /** Property key for GPU memory. */
    static final WritableObjectPropertyKey<GpuMemoryUsage> GPU_MEMORY =
            new WritableObjectPropertyKey<>();

    /**
     * All the property keys that can appear as a column. Sorted in order to appear in the context
     * menu and the header.
     */
    static final PropertyKey[] ALL_COLUMN_KEYS =
            new PropertyKey[] {
                TASK_NAME, MEMORY_FOOTPRINT, CPU, NETWORK_USAGE, PROCESS_ID, GPU_MEMORY
            };

    /**
     * Returns whether the initial sort ordering of this column should be ascending or not. Keep the
     * definition consistent with task_manager_columns.h for the columns defined in the file.
     */
    static boolean initialSortIsAscending(PropertyKey columnKey) {
        if (columnKey == TASK_NAME) {
            return true;
        } else if (columnKey == MEMORY_FOOTPRINT) {
            return false;
        } else if (columnKey == CPU) {
            return false;
        } else if (columnKey == NETWORK_USAGE) {
            return false;
        } else if (columnKey == PROCESS_ID) {
            return true;
        } else if (columnKey == GPU_MEMORY) {
            return false;
        }
        throw new IllegalArgumentException("column key " + columnKey + " not supported");
    }
}
