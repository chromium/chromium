// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.content.Context;
import android.util.AtomicFile;
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

    private static final String sBaseDirName = "attribution_reporting";
    private static final String sAttributionDataDirName =
            sBaseDirName + File.separator + "attribution_data";
    private static final String sEnumDirName = sBaseDirName + File.separator + "enums";
    private static class DirectoryHolder {
        private static File sBaseDirectory =
                ContextUtils.getApplicationContext().getDir(sBaseDirName, Context.MODE_PRIVATE);
        private static File sDataDirectory = new File(sBaseDirectory, sAttributionDataDirName);
        private static File sEnumDirectory = new File(sBaseDirectory, sEnumDirName);
    }

    private static final char VERSION_DELIMITER = '$';
    private static final char ENUM_DELIMITER = VERSION_DELIMITER;

    public ImpressionPersistentStoreFileManagerImpl() {}

    private String fileNameForPackage(String packageName, int version) {
        return packageName + VERSION_DELIMITER + version;
    }

    private String fileNameForEnum(String metricName, int enumValue) {
        return metricName + ENUM_DELIMITER + enumValue;
    }

    @Override
    public Pair<DataOutputStream, Long> getForPackage(String packageName, int version)
            throws IOException {
        if (!DirectoryHolder.sDataDirectory.exists()) DirectoryHolder.sDataDirectory.mkdirs();
        File file =
                new File(DirectoryHolder.sDataDirectory, fileNameForPackage(packageName, version));
        long fileSize = file.length();
        if (fileSize == 0L) file.createNewFile();
        return Pair.create(new DataOutputStream(new BufferedOutputStream(
                                   new FileOutputStream(file, true /* append */))),
                fileSize);
    }

    @Override
    public List<AttributionFileProperties<DataInputStream>> getAllAttributionFiles()
            throws IOException {
        List<AttributionFileProperties<DataInputStream>> fileProperties = new ArrayList<>();
        if (!DirectoryHolder.sDataDirectory.exists()) return fileProperties;
        for (File file : DirectoryHolder.sDataDirectory.listFiles()) {
            DataInputStream reader =
                    new DataInputStream(new BufferedInputStream(new FileInputStream(file)));
            int delimiterPos = file.getName().lastIndexOf(VERSION_DELIMITER);
            assert delimiterPos != -1;
            String packageName = file.getName().substring(0, delimiterPos);
            int version = Integer.parseInt(file.getName().substring(delimiterPos + 1));
            fileProperties.add(
                    new AttributionFileProperties<DataInputStream>(reader, packageName, version));
        }
        return fileProperties;
    }

    @Override
    public void incrementEnumMetric(String metricName, int enumValue) throws IOException {
        if (!DirectoryHolder.sEnumDirectory.exists()) DirectoryHolder.sEnumDirectory.mkdirs();
        // Use AtomicFile to avoid losing metrics in case of kernel panics or power loss. This can
        // end up being expensive at the long tail, but we don't have a better alternative.
        File rawFile =
                new File(DirectoryHolder.sEnumDirectory, fileNameForEnum(metricName, enumValue));
        AtomicFile file = new AtomicFile(rawFile);
        int currentValue = 0;
        if (rawFile.length() > 0L) {
            try (DataInputStream reader =
                            new DataInputStream(new BufferedInputStream(file.openRead()))) {
                currentValue = reader.readInt();
            }
        }
        FileOutputStream output = file.startWrite();
        try {
            DataOutputStream writer = new DataOutputStream(new BufferedOutputStream(output));
            writer.writeInt(currentValue + 1);
            writer.flush();
            file.finishWrite(output);
        } catch (Exception e) {
            file.failWrite(output);
            throw e;
        }
    }

    @Override
    public List<CachedEnumMetric> getCachedEnumMetrics() throws IOException {
        List<CachedEnumMetric> metrics = new ArrayList<>();
        if (!DirectoryHolder.sEnumDirectory.exists()) return metrics;
        for (File file : DirectoryHolder.sEnumDirectory.listFiles()) {
            try (DataInputStream reader = new DataInputStream(
                         new BufferedInputStream(new FileInputStream(file)))) {
                int delimiterPos = file.getName().lastIndexOf(ENUM_DELIMITER);
                assert delimiterPos != -1;
                String metricName = file.getName().substring(0, delimiterPos);
                int enumValue = Integer.parseInt(file.getName().substring(delimiterPos + 1));
                int count = reader.readInt();
                metrics.add(new CachedEnumMetric(metricName, enumValue, count));
            }
        }
        return metrics;
    }

    @Override
    public void clearAllData() {
        File[] files = DirectoryHolder.sDataDirectory.listFiles();
        if (files != null) {
            for (File file : files) file.delete();
        }

        files = DirectoryHolder.sEnumDirectory.listFiles();
        if (files != null) {
            for (File file : files) file.delete();
        }
    }
}
