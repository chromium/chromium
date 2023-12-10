// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.PathUtils;

import java.io.File;
import java.util.Arrays;

/** A Util class for operations on {@link ComponentsProviderService} serving directory. */
@JNINamespace("android_webview")
public class ComponentsProviderPathUtil {
    private static final String COMPONENTS_DIRECTORY_PATH = "components/cps";
    private static final String COMPONENT_UPDATE_SERVICE_DIRECTORY_PATH = "components/cus";

    /**
     * @return The absolute path to the serving directory that {@link ComponentsProviderService}
     *         uses to look components up.
     */
    @CalledByNative
    public static String getComponentsServingDirectoryPath() {
        return new File(PathUtils.getDataDirectory(), COMPONENTS_DIRECTORY_PATH).getAbsolutePath();
    }

    /** @return The absolute path to the directory where the update service stores components. */
    public static String getComponentUpdateServiceDirectoryPath() {
        return new File(PathUtils.getDataDirectory(), COMPONENT_UPDATE_SERVICE_DIRECTORY_PATH)
                .getAbsolutePath();
    }

    /**
     * Return the highest sequence number of the direct subdirectories in the given directory. It
     * looks up directories with the following name format {@code
     * <componentDirectoryPath>/<sequence-number>_<version>}.
     *
     * @param componentDirectoryPath the absolute path of the component directory.
     * @return the highest sequence number or 0 if the directory is empty or no valid directories
     *         that match the format.
     */
    @CalledByNative
    private static int getTheHighestSequenceNumber(String componentDirectoryPath) {
        File[] filesSorted = getComponentsNewestFirst(new File(componentDirectoryPath));
        if (filesSorted == null || filesSorted.length == 0) {
            return 0;
        }
        return sequenceNumberForDirectory(filesSorted[0]);
    }

    /**
     * List files under componentDirectory that are a directory and its name matches
     * <sequence_number>_<version>, where sequence number is composed only of numeric digits and
     * sort them in descending order of sequence numbers.
     *
     * @param componentDirectory the component directory that has components versions.
     * @return Sorted array of directories under {@code componentDirectory}, {@code null} if it's
     *         not a valid directory.
     */
    public static File[] getComponentsNewestFirst(File componentDirectory) {
        final File[] files =
                componentDirectory.listFiles(
                        file -> (file.isDirectory() && file.getName().matches("[0-9]+_.+")));

        if (files != null && files.length > 1) {
            // Sort the array in descending order of sequence numbers.
            Arrays.sort(
                    files,
                    (v1, v2) -> sequenceNumberForDirectory(v2) - sequenceNumberForDirectory(v1));
        }
        return files;
    }

    public static Integer sequenceNumberForDirectory(File directory) {
        String name = directory.getName();
        int separatorIndex = name.indexOf("_");
        return Integer.parseInt(name.substring(0, separatorIndex));
    }

    // Don't instaniate this class.
    private ComponentsProviderPathUtil() {}
}
