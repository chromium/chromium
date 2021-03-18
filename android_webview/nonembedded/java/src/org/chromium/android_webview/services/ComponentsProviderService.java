// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.Service;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.ResultReceiver;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.component_updater.IComponentsProviderService;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.Future;
import java.util.concurrent.FutureTask;

/**
 * A Service to fetch Components files in WebView and WebLayer.
 */
public class ComponentsProviderService extends Service {
    private static final String TAG = "AW_CPS";
    private static final String COMPONENTS_DIRECTORY_PATH = "components/cps";
    public static final int RESULT_OK = 0;
    public static final int RESULT_FAILED = 1;
    public static final String KEY_RESULT = "RESULT";

    private File mDirectory;
    private FutureTask<Void> mDeleteTask;

    private final IBinder mBinder = new IComponentsProviderService.Stub() {
        @Override
        public void getFilesForComponent(String componentId, ResultReceiver resultReceiver) {
            getFilesForComponentInternal(componentId, resultReceiver);
        }
    };

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public void onCreate() {
        mDirectory = new File(PathUtils.getDataDirectory(), COMPONENTS_DIRECTORY_PATH);
        if (!mDirectory.exists() && !mDirectory.mkdirs()) {
            Log.e(TAG, "Failed to create directory " + COMPONENTS_DIRECTORY_PATH);
            return;
        }

        cleanupOlderFiles();
    }

    private void cleanupOlderFiles() {
        final File[] components = mDirectory.listFiles();
        if (components == null || components.length == 0) {
            return;
        }
        final List<File> oldFiles = new LinkedList<>();
        for (File component : components) {
            final File[] versions = getComponentsNewestFirst(component);
            if (versions == null || versions.length == 0) {
                // This can happen if CUS created a parent directory but was killed before it could
                // move content into it. In this case there's nothing old to delete.
                continue;
            }
            // Add all directories except the newest (index 0) to oldFiles.
            oldFiles.addAll(Arrays.asList(versions).subList(1, versions.length));
        }

        // Delete old files in background.
        mDeleteTask = new FutureTask<>(() -> {
            for (File file : oldFiles) {
                if (!FileUtils.recursivelyDeleteFile(file, null)) {
                    Log.w(TAG, "Failed to delete " + file.getAbsolutePath());
                }
            }
            return null;
        });
        PostTask.postTask(TaskTraits.THREAD_POOL_BEST_EFFORT, mDeleteTask);
    }

    /**
     * This must be called after {@code onCreate()}, otherwise returns a {@code null} object.
     */
    @VisibleForTesting
    public Future<Void> getDeleteTaskForTesting() {
        return mDeleteTask;
    }

    private void getFilesForComponentInternal(String componentId, ResultReceiver resultReceiver) {
        // Note that there's no need to sanitize input because this method will check if there is an
        // existing folder under `mDirectory` with a name that equals the received `componentId`.
        // Because `mDirectory` is inside this application's data dir, only WebView can modify it.
        final File[] components = mDirectory.listFiles((dir, name) -> name.equals(componentId));
        if (components == null || components.length == 0) {
            resultReceiver.send(RESULT_FAILED, /* resultData = */ null);
            return;
        }
        assert components.length == 1 : "Only one directory should have the name " + componentId;

        final File[] versions = getComponentsNewestFirst(components[0]);
        if (versions == null || versions.length == 0) {
            // This can happen if CUS created a parent directory but was killed before it could
            // move content into it. In this case there's nothing old to delete.
            resultReceiver.send(RESULT_FAILED, /* resultData = */ null);
            return;
        }
        final File versionDirectory = versions[0];

        final HashMap<String, ParcelFileDescriptor> resultMap = new HashMap<>();
        try {
            recursivelyGetParcelFileDescriptors(
                    versionDirectory, versionDirectory.getAbsolutePath() + "/", resultMap);

            if (resultMap.isEmpty()) {
                Log.w(TAG, "No file descriptors found for " + componentId);
                resultReceiver.send(RESULT_FAILED, /* resultData = */ null);
                return;
            }

            final Bundle resultData = new Bundle();
            resultData.putSerializable(KEY_RESULT, resultMap);
            resultReceiver.send(RESULT_OK, resultData);
        } catch (IOException exception) {
            Log.w(TAG, exception.getMessage(), exception);
            resultReceiver.send(RESULT_FAILED, /* resultData = */ null);
        } finally {
            closeFileDescriptors(resultMap);
        }
    }

    private void recursivelyGetParcelFileDescriptors(File file, String pathPrefix,
            HashMap<String, ParcelFileDescriptor> resultMap) throws IOException {
        if (file.isDirectory()) {
            File[] files = file.listFiles();
            if (files != null) {
                for (File f : files) {
                    recursivelyGetParcelFileDescriptors(f, pathPrefix, resultMap);
                }
            }
        } else {
            resultMap.put(file.getAbsolutePath().replace(pathPrefix, ""),
                    ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY));
        }
    }

    private void closeFileDescriptors(HashMap<String, ParcelFileDescriptor> map) {
        assert map != null : "closeFileDescriptors called with a null map";

        for (ParcelFileDescriptor fileDescriptor : map.values()) {
            try {
                fileDescriptor.close();
            } catch (IOException exception) {
                Log.w(TAG, exception.getMessage());
            }
        }
    }

    private File[] getComponentsNewestFirst(File componentDirectory) {
        // List files under componentDirectory that are a directory and its name matches
        // <sequence_number>_<version>, where sequence number is composed only of numeric digits.
        final File[] files = componentDirectory.listFiles(
                file -> (file.isDirectory() && file.getName().matches("[0-9]+_.+")));

        if (files != null && files.length > 1) {
            // Sort the array in descending order of sequence numbers.
            Arrays.sort(files,
                    (v1, v2) -> sequenceNumberForDirectory(v2) - sequenceNumberForDirectory(v1));
        }
        return files;
    }

    private Integer sequenceNumberForDirectory(File directory) {
        String name = directory.getName();
        int separatorIndex = name.indexOf("_");
        return Integer.parseInt(name.substring(0, separatorIndex));
    }
}
