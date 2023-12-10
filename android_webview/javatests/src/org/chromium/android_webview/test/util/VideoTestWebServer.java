// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.util.Pair;

import org.chromium.net.test.util.TestWebServer;

import java.util.List;

/** This class is a WebServer provide video data. */
public class VideoTestWebServer {
    public static final String ONE_PIXEL_ONE_FRAME_WEBM_FILENAME = "one_pixel_one_frame.webm";
    public static final String ONE_PIXEL_ONE_FRAME_WEBM_BASE64 =
            "GkXfo0AgQoaBAUL3gQFC8oEEQvOBCEKCQAR3ZWJtQoeBAkKFgQIYU4BnQN8VSalmQCgq17FAAw9C"
                    + "QE2AQAZ3aGFtbXlXQUAGd2hhbW15RIlACECPQAAAAAAAFlSua0AxrkAu14EBY8WBAZyBACK1nEAD"
                    + "dW5khkAFVl9WUDglhohAA1ZQOIOBAeBABrCBlrqBlh9DtnVAdOeBAKNAboEAAIDyCACdASqWAJYA"
                    + "Pk0ci0WD+IBAAJiWlu4XdQTSq2H4MW0+sMO0gz8HMRe+0jRo0aNGjRo0aNGjRo0aNGjRo0aNGjRo"
                    + "0aNGjRo0aNGjRo0VAAD+/729RWRzH4mOZ9/O8Dl319afX4gsgAAA";
    private String mOnePixelOneFrameWebmURL;
    private TestWebServer mTestWebServer;

    public VideoTestWebServer() throws Exception {
        mTestWebServer = TestWebServer.start();
        List<Pair<String, String>> headers = getWebmHeaders(true);
        mOnePixelOneFrameWebmURL =
                mTestWebServer.setResponseBase64(
                        "/" + ONE_PIXEL_ONE_FRAME_WEBM_FILENAME,
                        ONE_PIXEL_ONE_FRAME_WEBM_BASE64,
                        headers);
    }

    /** @return the mOnePixelOneFrameWebmURL */
    public String getOnePixelOneFrameWebmURL() {
        return mOnePixelOneFrameWebmURL;
    }

    public TestWebServer getTestWebServer() {
        return mTestWebServer;
    }

    private static List<Pair<String, String>> getWebmHeaders(boolean disableCache) {
        return CommonResources.getContentTypeAndCacheHeaders("video/webm", disableCache);
    }
}
