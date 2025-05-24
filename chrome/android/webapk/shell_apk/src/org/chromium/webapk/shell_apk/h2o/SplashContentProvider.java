// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.ParcelFileDescriptor;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.webapk.lib.common.WebApkCommonUtils;
import org.chromium.webapk.shell_apk.WebApkSharedPreferences;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.util.concurrent.atomic.AtomicReference;

/** ContentProvider for screenshot of splash screen. */
@NullMarked
public class SplashContentProvider extends ContentProvider
        implements ContentProvider.PipeDataWriter<Void> {
    /** Holds value which gets cleared after {@link ExpiringData#CLEAR_DATA_INTERVAL_MS}. */
    private static class ExpiringData {
        /** Time in milliseconds after constructing the object to clear the cached data. */
        private static final int CLEAR_CACHED_DATA_INTERVAL_MS = 10000;

        private final byte[] mCachedData;
        private final Handler mHandler;

        public ExpiringData(byte[] cachedData, Runnable expiryTask) {
            mCachedData = cachedData;
            mHandler = new Handler();
            mHandler.postDelayed(expiryTask, CLEAR_CACHED_DATA_INTERVAL_MS);
        }

        public byte[] getCachedData() {
            return mCachedData;
        }

        public void removeCallbacks() {
            mHandler.removeCallbacksAndMessages(null);
        }
    }

    /**
     * Maximum size in bytes of screenshot to transfer to browser. The screenshot should be
     * downsampled to fit. Capping the maximum size of the screenshot decreases bitmap encoding time
     * and image transfer time.
     */
    public static final int MAX_TRANSFER_SIZE_BYTES = 1024 * 1024 * 12;

    /** The encoding type of the last image vended by the ContentProvider. */
    private static Bitmap.@Nullable CompressFormat sEncodingFormat;

    private static final AtomicReference<ExpiringData> sCachedSplashBytes = new AtomicReference<>();

    /** The URI handled by this content provider. */
    private String mContentProviderUri;

    /**
     * Temporarily caches the passed-in splash screen screenshot. To preserve memory, the cached
     * data is cleared after a delay.
     */
    public static void cache(
            Context context,
            byte @Nullable [] splashBytes,
            Bitmap.CompressFormat encodingFormat,
            int splashWidth,
            int splashHeight) {
        SharedPreferences.Editor editor = WebApkSharedPreferences.getPrefs(context).edit();
        editor.putInt(WebApkSharedPreferences.PREF_SPLASH_WIDTH, splashWidth);
        editor.putInt(WebApkSharedPreferences.PREF_SPLASH_HEIGHT, splashHeight);
        editor.apply();

        sEncodingFormat = encodingFormat;
        getAndSetCachedData(splashBytes);
    }

    public static void clearCache() {
        getAndSetCachedData(null);
    }

    /**
     * Sets the cached splash screen screenshot and returns the old one. Thread safety: Can be
     * called from any thread.
     */
    private static byte @Nullable [] getAndSetCachedData(byte @Nullable [] newSplashBytes) {
        ExpiringData newData = null;
        if (newSplashBytes != null) {
            newData = new ExpiringData(newSplashBytes, SplashContentProvider::clearCache);
        }
        ExpiringData oldCachedData = sCachedSplashBytes.getAndSet(newData);
        if (oldCachedData == null) return null;

        oldCachedData.removeCallbacks();
        return oldCachedData.getCachedData();
    }

    @Override
    public boolean onCreate() {
        mContentProviderUri =
                WebApkCommonUtils.generateSplashContentProviderUri(getContext().getPackageName());
        return true;
    }

    @Override
    public @Nullable ParcelFileDescriptor openFile(Uri uri, String mode)
            throws FileNotFoundException {
        if (uri != null && uri.toString().equals(mContentProviderUri)) {
            String mimeType = assumeNonNull(null); // TODO: Set this properly.
            return openPipeHelper(uri, mimeType, null, null, this);
        }
        return null;
    }

    @Override
    public void writeDataToPipe(
            ParcelFileDescriptor output,
            Uri uri,
            String mimeType,
            @Nullable Bundle opts,
            @Nullable Void unused) {
        try (OutputStream out = new FileOutputStream(output.getFileDescriptor())) {
            byte[] cachedSplashBytes = getAndSetCachedData(null);
            if (cachedSplashBytes != null) {
                out.write(cachedSplashBytes);
            } else {
                // One way that this case gets hit is when the WebAPK is brought to the foreground
                // via Android Recents after the Android OOM killer has killed the host browser but
                // not SplashActivity.
                Bitmap splashScreenshot = recreateAndScreenshotSplash();
                if (splashScreenshot != null) {
                    sEncodingFormat =
                            SplashUtils.selectBitmapEncoding(
                                    splashScreenshot.getWidth(), splashScreenshot.getHeight());
                    splashScreenshot.compress(sEncodingFormat, 100, out);
                }
            }
            out.flush();
        } catch (Exception e) {
        }
    }

    @Override
    public @Nullable String getType(Uri uri) {
        if (uri != null && uri.toString().equals(mContentProviderUri)) {
            if (sEncodingFormat == null) {
                Context context = getContext().getApplicationContext();
                SharedPreferences prefs = WebApkSharedPreferences.getPrefs(context);
                int splashWidth = prefs.getInt(WebApkSharedPreferences.PREF_SPLASH_WIDTH, -1);
                int splashHeight = prefs.getInt(WebApkSharedPreferences.PREF_SPLASH_HEIGHT, -1);
                sEncodingFormat = SplashUtils.selectBitmapEncoding(splashWidth, splashHeight);
            }
            if (sEncodingFormat == Bitmap.CompressFormat.PNG) {
                return "image/png";
            } else if (sEncodingFormat == Bitmap.CompressFormat.JPEG) {
                return "image/jpeg";
            }
        }
        return null;
    }

    @Override
    public int update(
            Uri uri,
            @Nullable ContentValues values,
            @Nullable String where,
            String @Nullable [] whereArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, @Nullable String selection, String @Nullable [] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Uri insert(Uri uri, @Nullable ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Cursor query(
            Uri uri,
            String @Nullable [] projection,
            @Nullable String selection,
            String @Nullable [] selectionArgs,
            @Nullable String sortOrder) {
        throw new UnsupportedOperationException();
    }

    /** Builds splashscreen at size that it was last displayed and screenshots it. */
    private @Nullable Bitmap recreateAndScreenshotSplash() {
        Context context = getContext().getApplicationContext();
        SharedPreferences prefs = WebApkSharedPreferences.getPrefs(context);
        int splashWidth = prefs.getInt(WebApkSharedPreferences.PREF_SPLASH_WIDTH, -1);
        int splashHeight = prefs.getInt(WebApkSharedPreferences.PREF_SPLASH_HEIGHT, -1);
        return SplashUtils.createAndImmediatelyScreenshotSplashView(
                context, splashWidth, splashHeight, MAX_TRANSFER_SIZE_BYTES);
    }
}
