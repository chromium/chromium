// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;

import org.chromium.base.FileUtils;
import org.chromium.components.aboutui.CreditUtilsJni;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Content provider for about:credits.
 * Used by SystemWebview and TrichromeWebview, but not Monochrome.
 */
public class LicenseContentProvider extends ContentProvider
        implements ContentProvider.PipeDataWriter<String> {
    public static final String LICENSES_URI_SUFFIX = "LicenseContentProvider/webview_licenses";
    public static final String LICENSES_CONTENT_TYPE = "text/html";
    private static boolean sLibraryLoaded;

    @Override
    public boolean onCreate() {
        sLibraryLoaded = WebViewApkApplication.ensureNativeInitialized();
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (uri != null && uri.toString().endsWith(LICENSES_URI_SUFFIX)) {
            return openPipeHelper(null, null, null, "webview_licenses.notice", this);
        }
        return null;
    }

    @Override
    public void writeDataToPipe(
            ParcelFileDescriptor output, Uri uri, String mimeType, Bundle opts, String filename) {
        if (sLibraryLoaded) {
            CreditUtilsJni.get().writeCreditsHtml(output.detachFd());
        } else {
            // Missing native library means we're the webview stub and licenses are stored as an
            // asset.
            try (InputStream in = getContext().getAssets().open(filename);
                    OutputStream out = new FileOutputStream(output.getFileDescriptor())) {
                FileUtils.copyStream(in, out);
            } catch (IOException ioException) {
                throw new RuntimeException(ioException);
            }
        }
    }

    @Override
    public String getType(Uri uri) {
        if (uri != null && uri.toString().endsWith(LICENSES_URI_SUFFIX)) {
            return LICENSES_CONTENT_TYPE;
        }
        return null;
    }

    @Override
    public int update(Uri uri, ContentValues values, String where, String[] whereArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Cursor query(
            Uri uri,
            String[] projection,
            String selection,
            String[] selectionArgs,
            String sortOrder) {
        throw new UnsupportedOperationException();
    }
}
