// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.os.Build;

import androidx.appsearch.app.AppSearchSession;
import androidx.appsearch.platformstorage.PlatformStorage;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Factory for creating AppSearch sessions which can be overridden in tests. */
@NullMarked
public class AppSearchStorageFactory {
    private static @Nullable AppSearchStorageFactory sInstance;

    public static AppSearchStorageFactory getInstance() {
        if (sInstance == null) {
            sInstance = new AppSearchStorageFactory();
        }
        return sInstance;
    }

    public static void setInstanceForTesting(@Nullable AppSearchStorageFactory factory) {
        sInstance = factory;
    }

    /**
     * Opens a new {@link AppSearchSession}. Returns null if AppSearch is not available on this
     * device.
     */
    public @Nullable ListenableFuture<AppSearchSession> createSearchSessionAsync(
            String databaseName) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            return null;
        }
        return PlatformStorage.createSearchSessionAsync(
                new PlatformStorage.SearchContext.Builder(
                                ContextUtils.getApplicationContext(), databaseName)
                        // This executor is used for creating the search session and ALL
                        // asynchronous `AppSearchSession` methods. These tasks are low priority
                        // and perform I/O.
                        .setWorkerExecutor(PostTask.getBackgroundBestEffortMayBlockExecutor())
                        .build());
    }
}
