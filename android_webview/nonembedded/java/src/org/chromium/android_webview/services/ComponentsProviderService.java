// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.services;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.ResultReceiver;
import android.system.ErrnoException;
import android.system.Os;

import androidx.annotation.Nullable;

import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.components.component_updater.IComponentsProviderService;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import javax.annotation.concurrent.GuardedBy;

/**
 * A Service to fetch Components files in WebView and WebLayer.
 */
public class ComponentsProviderService extends Service {
    private static final String TAG = "AW_CPS";
    private static final String COMPONENTS_DIRECTORY_PATH = "components/cps";
    private static final String SHARED_PREFERENCES_FILE_NAME = "ComponentsProviderService Versions";
    public static final int RESULT_OK = 0;
    public static final int RESULT_FAILED = 1;
    public static final String KEY_RESULT = "RESULT";

    // Maps componentIds to their version, which is also the relative path where they are installed.
    // Components are installed in: <data-dir>/components/cps/<component-id>/<compontent-version>.
    // The map is persisted to memory and loaded when the service is created.
    @GuardedBy("mLock")
    private final Map<String, String> mComponentVersions = new HashMap<>();
    private File mDirectory;
    private final Object mLock = new Object();

    private final IBinder mBinder = new IComponentsProviderService.Stub() {
        @Override
        public void getFilesForComponent(String componentId, ResultReceiver resultReceiver) {
            getFilesForComponentInternal(componentId, resultReceiver);
        }

        @Override
        public boolean onNewVersion(String componentId, String installPath, String version) {
            if (Binder.getCallingUid() != Process.myUid()) {
                throw new SecurityException("onNewVersion may only be called by WebView UID");
            }
            return onNewVersionInternal(componentId, installPath, version);
        }
    };

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public void onCreate() {
        mDirectory = new File(getFilesDir(), COMPONENTS_DIRECTORY_PATH);
        if (!mDirectory.exists()) {
            if (!mDirectory.mkdirs()) {
                Log.w(TAG, "Failed to create directory " + COMPONENTS_DIRECTORY_PATH);
            }
            return;
        }

        loadComponentVersionsMap();
    }

    private void loadComponentVersionsMap() {
        synchronized (mLock) {
            SharedPreferences sharedPreferences =
                    getSharedPreferences(SHARED_PREFERENCES_FILE_NAME, Context.MODE_PRIVATE);
            for (String key : sharedPreferences.getAll().keySet()) {
                String value = sharedPreferences.getString(key, null);
                assert value != null : "Map values should never be null";
                mComponentVersions.put(key, value);
                // TODO(crbug.com/1176248): delete older versions if there are any
            }
        }
    }

    private void saveComponent(String componentId, String version) {
        synchronized (mLock) {
            SharedPreferences sharedPreferences =
                    getSharedPreferences(SHARED_PREFERENCES_FILE_NAME, Context.MODE_PRIVATE);
            sharedPreferences.edit().putString(componentId, version).apply();
        }
    }

    private void getFilesForComponentInternal(String componentId, ResultReceiver resultReceiver) {
        // Note that there's no need to sanitize input because this method will check if the
        // componentId is contained in mComponentVersions map. Values are only added to that map in
        // onNewVersion, which we trust because it can only be called from WebView UID.
        synchronized (mLock) {
            final String componentVersion = mComponentVersions.get(componentId);
            if (componentVersion == null) {
                Log.w(TAG, "Component " + componentId + " version not found");
                resultReceiver.send(RESULT_FAILED, null);
                return;
            }

            File versionDir = getComponentDirectory(componentId, componentVersion);
            if (!versionDir.exists()) {
                Log.w(TAG, "Component " + componentId + " directory not found");
                mComponentVersions.remove(componentId);
                // Remove this component by setting its value to null, which is equivalent to
                // removing that key from SharedPreferences.
                saveComponent(componentId, null);
                resultReceiver.send(RESULT_FAILED, null);
                return;
            }

            final HashMap<String, ParcelFileDescriptor> resultMap = new HashMap<>();
            try {
                recursivelyGetParcelFileDescriptors(
                        versionDir, versionDir.getAbsolutePath() + "/", resultMap);
                Bundle resultData = new Bundle();
                resultData.putSerializable(KEY_RESULT, resultMap);
                resultReceiver.send(RESULT_OK, resultData);
            } catch (IOException exception) {
                Log.w(TAG, exception.getMessage(), exception);
                resultReceiver.send(RESULT_FAILED, null);
            } finally {
                closeFileDescriptors(resultMap);
            }
        }
    }

    // TODO(crbug.com/1176251) accept multiple components at once
    private boolean onNewVersionInternal(String componentId, String installPath, String version) {
        synchronized (mLock) {
            final String currentVersion = mComponentVersions.get(componentId);
            if (currentVersion != null && currentVersion.equals(version)) {
                // The same version is already installed, don't do anything.
                return false;
            }
            File targetDir = getComponentDirectory(componentId, version);
            // If target directory exists, delete it.
            if (targetDir.exists() && !FileUtils.recursivelyDeleteFile(targetDir, null)) {
                Log.w(TAG, "Directory already exists: " + targetDir.getAbsolutePath());
                return false;
            }
            // Create directories for new version.
            if (!targetDir.mkdirs()) {
                Log.w(TAG, "Failed to create directories: " + targetDir.getAbsolutePath());
                return false;
            }
            // Move new version.
            try {
                Os.rename(installPath, targetDir.getAbsolutePath());
                mComponentVersions.put(componentId, version);
                saveComponent(componentId, version);
                if (currentVersion != null) {
                    FileUtils.recursivelyDeleteFile(
                            getComponentDirectory(componentId, currentVersion), null);
                }
                return true;
            } catch (ErrnoException exception) {
                Log.w(TAG, "Failed to move new version of " + componentId, exception);
                return false;
            }
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

    private File getComponentDirectory(String componentId, String version) {
        return new File(mDirectory, componentId + "/" + version + "/");
    }
}
