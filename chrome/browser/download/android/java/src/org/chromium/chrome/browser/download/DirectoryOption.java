// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Denotes a given option for directory selection; includes name, location, and space.
 */
public class DirectoryOption {
    // Type to track user's selection of directory option. This enum is used in histogram and must
    // match DownloadLocationDirectoryType in enums.xml, so don't delete or reuse values.
    @IntDef({DownloadLocationDirectoryType.DEFAULT, DownloadLocationDirectoryType.ADDITIONAL,
            DownloadLocationDirectoryType.ERROR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DownloadLocationDirectoryType {
        int DEFAULT = 0;
        int ADDITIONAL = 1;
        int ERROR = 2;

        int NUM_ENTRIES = 3;
    }

    /**
     * Name of the current download directory.
     */
    public String name;

    /**
     * The absolute path of the download location.
     */
    public final String location;

    /**
     * The available space in this download directory.
     */
    public final long availableSpace;

    /**
     * The total disk space of the partition.
     */
    public final long totalSpace;

    /**
     * The type of the directory option.
     */
    public final @DownloadLocationDirectoryType int type;

    public DirectoryOption(String name, String location, long availableSpace, long totalSpace,
            @DownloadLocationDirectoryType int type) {
        this(location, availableSpace, totalSpace, type);
        this.name = name;
    }

    public DirectoryOption(String location, long availableSpace, long totalSpace,
            @DownloadLocationDirectoryType int type) {
        this.location = location;
        this.availableSpace = availableSpace;
        this.totalSpace = totalSpace;
        this.type = type;
    }

    @Override
    public Object clone() {
        DirectoryOption directoryOption = new DirectoryOption(
                this.name, this.location, this.availableSpace, this.totalSpace, this.type);
        return directoryOption;
    }
}
