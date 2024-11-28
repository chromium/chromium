// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
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

    /** Property key for task name. */
    static final WritableObjectPropertyKey<String> TASK_NAME = new WritableObjectPropertyKey<>();

    /** Property key for memory footprint. */
    static final WritableLongPropertyKey MEMORY_FOOTPRINT = new WritableLongPropertyKey();

    /** Property key for cpu. */
    static final WritableFloatPropertyKey CPU = new WritableFloatPropertyKey();

    /** Property key for process id. */
    static final WritableLongPropertyKey PROCESS_ID = new WritableLongPropertyKey();

    /** Property key for whether the task is selected. */
    static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();
}
