// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Bitmap;
import android.util.Log;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.util.Random;

/**
 * This class takes advantage of shouldInterceptRequest(), returns the bitmap from
 * WebChromeClient.getDefaultVidoePoster() when the mDefaultVideoPosterURL is requested.
 *
 * The shouldInterceptRequest is used to get the default video poster, if the url is
 * the mDefaultVideoPosterURL.
 */
public class DefaultVideoPosterRequestHandler {
    private static InputStream getInputStream(final AwContentsClient contentClient)
            throws IOException {
        final PipedInputStream inputStream = new PipedInputStream();
        final PipedOutputStream outputStream = new PipedOutputStream(inputStream);

        // Send the request to UI thread to callback to the client, and if it provides a
        // valid bitmap bounce on to the worker thread pool to compress it into the piped
        // input/output stream.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            final Bitmap defaultVideoPoster = contentClient.getDefaultVideoPoster();
            if (defaultVideoPoster == null) {
                closeOutputStream(outputStream);
                return;
            }
            PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
                try {
                    defaultVideoPoster.compress(Bitmap.CompressFormat.PNG, 100,
                            outputStream);
                    outputStream.flush();
                } catch (IOException e) {
                    Log.e(TAG, null, e);
                } finally {
                    closeOutputStream(outputStream);
                }
            });
        });
        return inputStream;
    }

    private static void closeOutputStream(OutputStream outputStream) {
        try {
            outputStream.close();
        } catch (IOException e) {
            Log.e(TAG, null, e);
        }
    }

    private static final String TAG = "DefaultVideoPosterRequestHandler";
    private String mDefaultVideoPosterURL;
    private AwContentsClient mContentClient;

    public DefaultVideoPosterRequestHandler(AwContentsClient contentClient) {
        mDefaultVideoPosterURL = generateDefaulVideoPosterURL();
        mContentClient = contentClient;
    }

    /**
     * Used to get the image if the url is mDefaultVideoPosterURL.
     *
     * @param url the url requested
     * @return AwWebResourceResponse which caller can get the image if the url is
     * the default video poster URL, otherwise null is returned.
     */
    public AwWebResourceResponse shouldInterceptRequest(final String url) {
        if (!mDefaultVideoPosterURL.equals(url)) return null;

        try {
            return new AwWebResourceResponse("image/png", null, getInputStream(mContentClient));
        } catch (IOException e) {
            Log.e(TAG, null, e);
            return null;
        }
    }

    public String getDefaultVideoPosterURL() {
        return mDefaultVideoPosterURL;
    }

    /**
     * @return a unique URL which has little chance to be used by application.
     */
    private static String generateDefaulVideoPosterURL() {
        Random randomGenerator = new Random();
        String path = String.valueOf(randomGenerator.nextLong());
        // The scheme of this URL should be kept in sync with kAndroidWebViewVideoPosterScheme
        // on the native side (see android_webview/common/url_constants.h)
        return "android-webview-video-poster:default_video_poster/" + path;
    }
}
