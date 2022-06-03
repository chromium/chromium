// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.content.Context;
import android.util.Pair;

import org.chromium.base.ContextUtils;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Responsible for managing the file/folder structure for the attribution data cached to disk.
 */
public class ImpressionPersistentStoreFileManagerImpl
        implements ImpressionPersistentStoreFileManager<DataOutputStream, DataInputStream> {
    private static final String TAG = "ImpressionFileManager";

    private static final String sBaseDirName = "attribution_reporting_temporary_storage";
    private static class BaseStorageDirectoryHolder {
        private static File sDirectory =
                ContextUtils.getApplicationContext().getDir(sBaseDirName, Context.MODE_PRIVATE);
    }

    private static final char VERSION_DELIMITER = '$';

    public ImpressionPersistentStoreFileManagerImpl() {}

    private File getOrCreateBaseStorageDirectory() {
        return BaseStorageDirectoryHolder.sDirectory;
    }

    private String fileNameForPackage(String packageName, int version) {
        return packageName + VERSION_DELIMITER + version;
    }

    @Override
    public Pair<DataOutputStream, Long> getForPackage(String packageName, int version)
            throws IOException {
        File file = new File(
                getOrCreateBaseStorageDirectory(), fileNameForPackage(packageName, version));
        long fileSize = file.length();
        if (fileSize == 0L) file.createNewFile();
        return Pair.create(
                new DataOutputStream(new BufferedOutputStream(new FileOutputStream(file, true))),
                fileSize);
    }

    @Override
    public List<FileProperties<DataInputStream>> getAllFiles() throws IOException {
        File dir = getOrCreateBaseStorageDirectory();
        List<FileProperties<DataInputStream>> fileProperties = new ArrayList<>();
        for (File file : dir.listFiles()) {
            DataInputStream reader =
                    new DataInputStream(new BufferedInputStream(new FileInputStream(file)));
            int delimiterPos = file.getName().lastIndexOf(VERSION_DELIMITER);
            assert delimiterPos != -1;
            String packageName = file.getName().substring(0, delimiterPos);
            int version = Integer.parseInt(file.getName().substring(delimiterPos + 1));
            fileProperties.add(new FileProperties<DataInputStream>(reader, packageName, version));
        }
        return fileProperties;
    }

    @Override
    public void clearAllData() {
        File dir = getOrCreateBaseStorageDirectory();
        for (File file : dir.listFiles()) {
            file.delete();
        }
    }
}
