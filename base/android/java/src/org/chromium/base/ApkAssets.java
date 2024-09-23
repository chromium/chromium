// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.BuildConfig;

import java.io.IOException;
import java.util.Arrays;

/**
 * A utility class to retrieve references to uncompressed assets insides the apk. A reference is
 * defined as tuple (file descriptor, offset, size) enabling direct mapping without deflation. This
 * can be used even within the renderer process, since it just dup's the apk's fd.
 */
@JNINamespace("base::android")
public class ApkAssets {
    private static final String TAG = "ApkAssets";

    // This isn't thread safe, but that's ok because it's only used for debugging.
    // Note reference operations are atomic so there is no security issue.
    private static String sLastError;

    @CalledByNative
    public static long[] open(String apkSubpath, String splitName) {
        apkSubpath = maybeAddSuffix(apkSubpath);
        sLastError = null;
        AssetFileDescriptor afd = null;
        try {
            Context context = ContextUtils.getApplicationContext();
            if (!TextUtils.isEmpty(splitName) && BundleUtils.isIsolatedSplitInstalled(splitName)) {
                context = BundleUtils.createIsolatedSplitContext(context, splitName);
            }
            AssetManager manager = context.getAssets();
            afd = manager.openNonAssetFd(apkSubpath);
            return new long[] {
                afd.getParcelFileDescriptor().detachFd(), afd.getStartOffset(), afd.getLength()
            };
        } catch (IOException e) {
            sLastError =
                    "Error while loading asset " + apkSubpath + " from " + splitName + ": " + e;
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
            if (!e.getMessage().equals("") && !e.getMessage().equals(apkSubpath)) {
                Log.e(TAG, sLastError);
            }
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

    private static String maybeAddSuffix(String apkSubpath) {
        if (BuildConfig.APK_ASSETS_SUFFIX != null
                && Arrays.binarySearch(BuildConfig.APK_ASSETS_SUFFIXED_LIST, apkSubpath) >= 0) {
            apkSubpath += BuildConfig.APK_ASSETS_SUFFIX;
        }
        return apkSubpath;
    }

    public static boolean exists(String apkSubpath) {
        AssetManager manager = ContextUtils.getApplicationContext().getAssets();
        try (AssetFileDescriptor afd = manager.openNonAssetFd(maybeAddSuffix(apkSubpath))) {
            return true;
        } catch (IOException e2) {
        }
        return false;
    }

    @CalledByNative
    private static String takeLastErrorString() {
        String rv = sLastError;
        sLastError = null;
        return rv;
    }
}
