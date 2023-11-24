// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.download.home.rename;

import org.jni_zero.NativeMethods;

import org.chromium.base.FileUtils;

/** A class containing some utility static methods for rename. */
public class RenameUtils {
    private static boolean sIsDisabledNativeForTesting;

    /**
     * Determine the extension of a downloaded item.
     *
     * @param fileName input to determine the extension.
     * @return String representing the extension part of fileName.
     * Return empty if there is no extension, otherwise return extension part starting with
     * separator. e.g. it will return ".jpg" for the path "foo/bar.jpg", and return ".tar.gz" for
     * the path "foo/bar.tar.gz".
     */
    public static String getFileExtension(String fileName) {
        return sIsDisabledNativeForTesting
                ? FileUtils.getExtension(fileName)
                : RenameUtilsJni.get().getFileExtension(fileName);
    }

    /** Disables the native APIs. This is only intended for testing purposes. */
    public static void disableNativeForTesting() {
        sIsDisabledNativeForTesting = true;
    }

    @NativeMethods
    interface Natives {
        String getFileExtension(String fileName);
    }
}
