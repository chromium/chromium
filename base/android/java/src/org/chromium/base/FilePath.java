// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.annotations.NullMarked;

/** Limited Java version of C++ base::FilePath. */
@NullMarked
public class FilePath {
    private final String mPath;

    private FilePath(String path) {
        mPath = path;
    }

    /**
     * Constructs a {@link FilePath} from the given string path, truncating at any null char.
     *
     * @param path posix path.
     */
    public static FilePath from(String path) {
        int nullIndex = path.indexOf('\0');
        return new FilePath(nullIndex == -1 ? path : path.substring(0, nullIndex));
    }

    /**
     * Returns the string value of the path.
     *
     * @return The string representation of this path.
     */
    public String value() {
        return mPath;
    }

    /**
     * Returns whether the path starts with '/'.
     *
     * @return True if the path is absolute, false otherwise.
     */
    public boolean isAbsolute() {
        return mPath.length() > 0 && mPath.charAt(0) == '/';
    }

    /**
     * Returns whether the path references a parent directory with a ".." component.
     *
     * @return True if the path contains a parent directory reference, false otherwise.
     */
    public boolean referencesParent() {
        return mPath.equals("..")
                || mPath.startsWith("../")
                || mPath.contains("/../")
                || mPath.endsWith("/..");
    }
}
