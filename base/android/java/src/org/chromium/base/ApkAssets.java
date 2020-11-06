// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;

/**
 * A utility class to retrieve references to uncompressed assets insides the apk. A reference is
 * defined as tuple (file descriptor, offset, size) enabling direct mapping without deflation.
 * This can be used even within the renderer process, since it just dup's the apk's fd.
 */
@JNINamespace("base::android")
public class ApkAssets {
    private static final String LOGTAG = "ApkAssets";

    @CalledByNative
    public static long[] open(String fileName, String splitName) {
        AssetFileDescriptor afd = null;
        try {
            Context context = ContextUtils.getApplicationContext();
            if (!TextUtils.isEmpty(splitName)
                    && BundleUtils.isIsolatedSplitInstalled(context, splitName)) {
                context = BundleUtils.createIsolatedSplitContext(context, splitName);
            }
            AssetManager manager = context.getAssets();
            afd = manager.openNonAssetFd(fileName);
            return new long[] {afd.getParcelFileDescriptor().detachFd(), afd.getStartOffset(),
                    afd.getLength()};
        } catch (IOException e) {
            // As a general rule there's no point logging here because the caller should handle
            // receiving an fd of -1 sensibly, and the log message is either mirrored later, or
            // unwanted (in the case where a missing file is expected), or wanted but will be
            // ignored, as most non-fatal logs are.
            // It makes sense to log here when the file exists, but is unable to be opened as an fd
            // because (for example) it is unexpectedly compressed in an apk. In that case, the log
            // message might save someone some time working out what has gone wrong.
            // For that reason, we only suppress the message when the exception message doesn't look
            // informative (Android framework passes the filename as the message on actual file not
            // found, and the empty string also wouldn't give any useful information for debugging).
            if (!e.getMessage().equals("") && !e.getMessage().equals(fileName)) {
                Log.e(LOGTAG, "Error while loading asset " + fileName + ": " + e);
            }
            return new long[] {-1, -1, -1};
        } finally {
            try {
                if (afd != null) {
                    afd.close();
                }
            } catch (IOException e2) {
                Log.e(LOGTAG, "Unable to close AssetFileDescriptor", e2);
            }
        }
    }
}
