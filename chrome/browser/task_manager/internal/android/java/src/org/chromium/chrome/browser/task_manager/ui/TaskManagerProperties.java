// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
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
        RowType.HEADER,
        RowType.TASK,
    })
    @Retention(RetentionPolicy.SOURCE)
    static @interface RowType {
        /**
         * Represents the header of the table. The item with this key has property keys
         * corresponding to the attributes each task item should have.
         */
        int HEADER = 1;

        /** Represents a task. Each item with this key has prorperties of the corresponding task. */
        int TASK = 2;
    }

    /** Property key for the columns of the header. */
    static final WritableObjectPropertyKey<PropertyKey[]> COLUMNS =
            new WritableObjectPropertyKey<>();

    /** Property key for task id. */
    static final WritableLongPropertyKey TASK_ID = new WritableLongPropertyKey();

    /** Property key for task name. */
    static final WritableObjectPropertyKey<String> TASK_NAME = new WritableObjectPropertyKey<>();

    /** Property key for memory footprint. */
    static final WritableLongPropertyKey MEMORY_FOOTPRINT = new WritableLongPropertyKey();

    /** Property key for cpu. */
    static final WritableFloatPropertyKey CPU = new WritableFloatPropertyKey();

    /** Property key for process id. */
    static final WritableLongPropertyKey PROCESS_ID = new WritableLongPropertyKey();
}
