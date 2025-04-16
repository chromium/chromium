// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Bitmap;
import android.util.Log;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.util.Random;

/**
 * This class takes advantage of shouldInterceptRequest(), returns the bitmap from
 * WebChromeClient.getDefaultVidoePoster() when the mDefaultVideoPosterUrl is requested.
 *
 * <p>The shouldInterceptRequest is used to get the default video poster, if the url is the
 * mDefaultVideoPosterUrl.
 */
@Lifetime.WebView
@NullMarked
public class DefaultVideoPosterRequestHandler {
    private static InputStream getInputStream(final AwContentsClient contentClient)
            throws IOException {
        final PipedInputStream inputStream = new PipedInputStream();
        final PipedOutputStream outputStream = new PipedOutputStream(inputStream);

        // Send the request to UI thread to callback to the client, and if it provides a
        // valid bitmap bounce on to the worker thread pool to compress it into the piped
        // input/output stream.
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    final Bitmap defaultVideoPoster = contentClient.getDefaultVideoPoster();
                    if (defaultVideoPoster == null) {
                        closeOutputStream(outputStream);
                        return;
                    }
                    PostTask.postTask(
                            TaskTraits.BEST_EFFORT_MAY_BLOCK,
                            () -> {
                                try {
                                    defaultVideoPoster.compress(
                                            Bitmap.CompressFormat.PNG, 100, outputStream);
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
    private final String mDefaultVideoPosterUrl;
    private final AwContentsClient mContentClient;

    public DefaultVideoPosterRequestHandler(AwContentsClient contentClient) {
        mDefaultVideoPosterUrl = generateDefaulVideoPosterUrl();
        mContentClient = contentClient;
    }

    /**
     * Used to get the image if the url is mDefaultVideoPosterUrl.
     *
     * @param url the url requested
     * @return WebResourceResponseInfo which caller can get the image if the url is the default
     *     video poster URL, otherwise null is returned.
     */
    public @Nullable WebResourceResponseInfo shouldInterceptRequest(final String url) {
        if (!mDefaultVideoPosterUrl.equals(url)) return null;

        try {
            return new WebResourceResponseInfo("image/png", null, getInputStream(mContentClient));
        } catch (IOException e) {
            Log.e(TAG, null, e);
            return null;
        }
    }

    public String getDefaultVideoPosterUrl() {
        return mDefaultVideoPosterUrl;
    }

    /**
     * @return a unique URL which has little chance to be used by application.
     */
    private static String generateDefaulVideoPosterUrl() {
        Random randomGenerator = new Random();
        String path = String.valueOf(randomGenerator.nextLong());
        // The scheme of this URL should be kept in sync with kAndroidWebViewVideoPosterScheme
        // on the native side (see android_webview/common/url_constants.h)
        return "android-webview-video-poster:default_video_poster/" + path;
    }
}
