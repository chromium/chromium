// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import static org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory.getOriginalApplicationContext;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.io.IOException;

/**
 * Used by aw_media_url_interceptor to read assets from the embedding application.
 * TODO(crbug.com/347649768): Remove this class if it isn't being used.
 */
@JNINamespace("android_webview")
public class AwAssetReader {
    private static final String TAG = "AwAssetReader";

    @CalledByNative
    public static long[] open(String fileName) {
        AssetFileDescriptor afd = null;
        try {
            // Use the embedding app's Context so that we can read assets properly.
            Context context = getOriginalApplicationContext(ContextUtils.getApplicationContext());
            AssetManager manager = context.getAssets();
            afd = manager.openNonAssetFd(fileName);
            return new long[] {
                afd.getParcelFileDescriptor().detachFd(), afd.getStartOffset(), afd.getLength()
            };
        } catch (IOException e) {
            Log.e(TAG, "Error while loading asset " + fileName + ": " + e);
            return new long[] {-1, -1, -1};
        } finally {
            try {
                if (afd != null) {
                    afd.close();
                }
            } catch (IOException e2) {
                Log.e(TAG, "Unable to close AssetFileDescriptor", e2);
            }
        }
    }
}
