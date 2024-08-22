// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.net.Uri;

import java.io.File;

/** This class provides methods to access content URI schemes. */
public abstract class FileProviderUtils {
    private static final String TAG = "FileProviderUtils";
    private static FileProviderUtil sFileProviderUtil;

    // Guards access to sFileProviderUtil.
    private static final Object sLock = new Object();

    /**
     * Provides functionality to translate a file into a content URI for use with a content
     * provider.
     */
    public interface FileProviderUtil {
        /**
         * Generate a content URI from the given file.
         *
         * @param file The file to be translated.
         */
        Uri getContentUriFromFile(File file);
    }

    // Prevent instantiation.
    private FileProviderUtils() {}

    public static void setFileProviderUtil(FileProviderUtil util) {
        synchronized (sLock) {
            sFileProviderUtil = util;
        }
    }

    /**
     * Get a URI for |file| which has the image capture. This function assumes that path of |file|
     * is based on the result of UiUtils.getDirectoryForImageCapture().
     *
     * @param file image capture file.
     * @return URI for |file|.
     * @throws IllegalArgumentException when the given File is outside the paths supported by the
     *     provider.
     */
    public static Uri getContentUriFromFile(File file) {
        synchronized (sLock) {
            if (sFileProviderUtil != null) {
                return sFileProviderUtil.getContentUriFromFile(file);
            }
        }
        return null;
    }
}
