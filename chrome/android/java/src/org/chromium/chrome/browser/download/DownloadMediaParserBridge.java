// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.base.Callback;
import org.chromium.base.annotations.NativeMethods;

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
        mNativeDownloadMediaParserBridge = DownloadMediaParserBridgeJni.get().init(
                DownloadMediaParserBridge.this, mimeType, filePath, callback);
    }

    /**
     * Destroys the native object of DownloadMediaParser. This will result in the utility process
     * being destroyed.
     */
    public void destroy() {
        DownloadMediaParserBridgeJni.get().destroy(
                mNativeDownloadMediaParserBridge, DownloadMediaParserBridge.this);
        mNativeDownloadMediaParserBridge = 0;
    }

    /**
     * Starts to parse a media file to retrieve media metadata and video thumbnail.
     */
    public void start() {
        if (mNativeDownloadMediaParserBridge != 0) {
            DownloadMediaParserBridgeJni.get().start(
                    mNativeDownloadMediaParserBridge, DownloadMediaParserBridge.this);
        }
    }

    @NativeMethods
    interface Natives {
        long init(DownloadMediaParserBridge caller, String mimeType, String filePath,
                Callback<DownloadMediaData> callback);
        void destroy(long nativeDownloadMediaParserBridge, DownloadMediaParserBridge caller);
        void start(long nativeDownloadMediaParserBridge, DownloadMediaParserBridge caller);
    }
}
