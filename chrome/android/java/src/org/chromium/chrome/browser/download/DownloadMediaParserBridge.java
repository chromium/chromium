// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.base.Callback;

/**
 * A JNI bridge that owns a native side DownloadMediaParser, which parses media file safely in an
 * utility process.
 */
public class DownloadMediaParserBridge {
    private long mNativeDownloadMediaParserBridge;

    /** Creates a media parser to analyze media metadata and retrieve thumbnails.
     * @param mimeType The mime type of the media file.
     * @param filePath The absolute path of the media file.
     * @param totalSize Total size of the media file.
     * @param callback Callback to get the result.
     */
    public DownloadMediaParserBridge(
            String mimeType, String filePath, Callback<DownloadMediaData> callback) {
        mNativeDownloadMediaParserBridge = nativeInit(mimeType, filePath, callback);
    }

    /**
     * Destroys the native object of DownloadMediaParser. This will result in the utility process
     * being destroyed.
     */
    public void destory() {
        nativeDestory(mNativeDownloadMediaParserBridge);
        mNativeDownloadMediaParserBridge = 0;
    }

    /**
     * Starts to parse a media file to retrieve media metadata and video thumbnail.
     */
    public void start() {
        if (mNativeDownloadMediaParserBridge != 0) {
            nativeStart(mNativeDownloadMediaParserBridge);
        }
    }

    private native long nativeInit(
            String mimeType, String filePath, Callback<DownloadMediaData> callback);
    private native void nativeDestory(long nativeDownloadMediaParserBridge);
    private native void nativeStart(long nativeDownloadMediaParserBridge);
}
