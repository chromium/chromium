// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thumbnail.generator;

import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;

/**
 * A JNI bridge for testing native ThumbnailMediaParser, which parses media file safely in an
 * utility process.
 */
public class ThumbnailMediaParserBridge {
    private ThumbnailMediaParserBridge() {}

    /**
     * Starts to parse a media file to retrieve media metadata and video thumbnail.
     * @param mimeType The mime type of the media file.
     * @param filePath The absolute path of the media file.
     * @param totalSize Total size of the media file.
     * @param callback Callback to get the result.
     *
     */
    public static void parse(
            String mimeType, String filePath, Callback<ThumbnailMediaData> callback) {
        ThumbnailMediaParserBridgeJni.get().parse(mimeType, filePath, callback);
    }

    @NativeMethods
    interface Natives {
        void parse(String mimeType, String filePath, Callback<ThumbnailMediaData> callback);
    }
}
