// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;

import java.io.File;
import java.io.IOException;

/** Methods for determining and recording the storage system type (e.g., eMMC, UFS). */
@NullMarked
public class StorageSystem {
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    public @interface StorageType {
        int UFS = 0;
        int EMMC = 1;
        int UNKNOWN = 2;
        int UNDETERMINED = 3;
        int COUNT = 4;
    }

    /** Asynchronously determines the storage type and records it to a UMA histogram. */
    public static void recordStorageType() {
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    @StorageType int storageType = getStorageType();
                    RecordHistogram.recordEnumeratedHistogram(
                            "Android.StorageSystem.Type", storageType, StorageType.COUNT);
                });
    }

    /**
     * Determines the storage type by inspecting system files.
     *
     * @return The detected storage type.
     */
    private static @StorageType int getStorageType() {
        try {
            String userdataBlock = getResolvedLink("/dev/block/by-name/userdata");
            if (userdataBlock == null) {
                return StorageType.UNKNOWN;
            }

            // Remove the "/dev/block/" part.
            userdataBlock = new File(userdataBlock).getName();

            if (userdataBlock.startsWith("mmc")) {
                return StorageType.EMMC;
            }

            String sysfsLink = getResolvedLink("/sys/class/block/" + userdataBlock);
            if (sysfsLink == null) {
                return StorageType.UNKNOWN;
            }

            if (sysfsLink.contains("/host0/")) {
                return StorageType.UFS;
            }

            return StorageType.UNDETERMINED;
        } catch (Exception e) {
            return StorageType.UNKNOWN;
        }
    }

    private static @Nullable String getResolvedLink(String path) {
        try {
            File file = new File(path);
            if (!file.exists()) {
                return null;
            }
            return file.getCanonicalPath();
        } catch (IOException e) {
            return null;
        }
    }
}
