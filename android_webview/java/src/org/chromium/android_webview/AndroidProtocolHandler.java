// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import static org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory.getOriginalApplicationContext;

import android.content.Context;
import android.content.res.AssetManager;
import android.net.Uri;
import android.util.Log;
import android.util.TypedValue;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ContextUtils;
import org.chromium.url.GURL;

import java.io.IOException;
import java.io.InputStream;
import java.net.URLConnection;
import java.util.List;
import java.util.zip.GZIPInputStream;

/**
 * Implements the Java side of Android URL protocol jobs.
 * See android_protocol_handler.cc.
 */
@Lifetime.Singleton
@JNINamespace("android_webview")
public class AndroidProtocolHandler {
    private static final String TAG = "AndroidProtocolHandler";

    // Supported URL schemes. This needs to be kept in sync with
    // clank/native/framework/chrome/url_request_android_job.cc.
    private static final String FILE_SCHEME = "file";
    private static final String CONTENT_SCHEME = "content";

    /**
     * Open an InputStream for an Android resource.
     *
     * @param url The url to load.
     * @return An InputStream to the Android resource.
     */
    @CalledByNative
    public static InputStream open(GURL url) {
        Uri uri = verifyUrl(url);
        if (uri == null) {
            return null;
        }
        InputStream stream = openByScheme(uri);
        if (stream != null && uri.getLastPathSegment().endsWith(".svgz")) {
            try {
                stream = new GZIPInputStream(stream);
            } catch (IOException e) {
                Log.e(TAG, "Error decompressing " + uri + " - " + e.getMessage());
                return null;
            }
        }
        return stream;
    }

    private static InputStream openByScheme(Uri uri) {
        try {
            if (uri.getScheme().equals(FILE_SCHEME)) {
                String path = uri.getPath();
                if (path.startsWith(AndroidProtocolHandlerJni.get().getAndroidAssetPath())) {
                    return openAsset(uri);
                } else if (path.startsWith(
                        AndroidProtocolHandlerJni.get().getAndroidResourcePath())) {
                    return openResource(uri);
                }
            } else if (uri.getScheme().equals(CONTENT_SCHEME)) {
                return openContent(uri);
            }
        } catch (Exception ex) {
            Log.e(TAG, "Error opening inputstream: " + uri);
        }
        return null;
    }

    // Assumes input string is in the format "foo.bar.baz" and strips out the last component.
    // Returns null on failure.
    private static String removeOneSuffix(String input) {
        if (input == null) return null;
        int lastDotIndex = input.lastIndexOf('.');
        if (lastDotIndex == -1) return null;
        return input.substring(0, lastDotIndex);
    }

    private static Class<?> getClazz(String packageName, String assetType)
            throws ClassNotFoundException {
        return ContextUtils.getApplicationContext()
                .getClassLoader()
                .loadClass(packageName + ".R$" + assetType);
    }

    // file://android_res/ has no choice but to do name-based resource lookups
    @SuppressWarnings("DiscouragedApi")
    private static int getFieldId(String assetType, String assetName)
            throws ClassNotFoundException, NoSuchFieldException, IllegalAccessException {
        Context appContext = ContextUtils.getApplicationContext();
        String packageName = appContext.getPackageName();
        int id =
                getOriginalApplicationContext(appContext)
                        .getResources()
                        .getIdentifier(assetName, assetType, packageName);
        if (id != 0) {
            return id;
        }
        // Fail back to reflection if getIdentifier fails, see https://crbug.com/923956.
        Class<?> clazz = null;
        try {
            clazz = getClazz(packageName, assetType);
        } catch (ClassNotFoundException e) {
            // Strip out components from the end so gradle generated application suffix such as
            // com.example.my.pkg.pro works. This is by no means bulletproof.
            while (clazz == null) {
                packageName = removeOneSuffix(packageName);
                // Throw original exception which contains the entire package id.
                if (packageName == null) throw e;
                try {
                    clazz = getClazz(packageName, assetType);
                } catch (ClassNotFoundException ignored) {
                    // Strip and try again.
                }
            }
        }
        java.lang.reflect.Field field = clazz.getField(assetName);
        id = field.getInt(null);

        return id;
    }

    private static int getValueType(int fieldId) {
        TypedValue value = new TypedValue();
        getOriginalApplicationContext(ContextUtils.getApplicationContext())
                .getResources()
                .getValue(fieldId, value, true);
        return value.type;
    }

    private static InputStream openResource(Uri uri) {
        assert uri.getScheme().equals(FILE_SCHEME);
        assert uri.getPath() != null;
        assert uri.getPath().startsWith(AndroidProtocolHandlerJni.get().getAndroidResourcePath());
        // The path must be of the form "/android_res/asset_type/asset_name.ext".
        List<String> pathSegments = uri.getPathSegments();
        if (pathSegments.size() != 3) {
            Log.e(TAG, "Incorrect resource path: " + uri);
            return null;
        }
        String assetPath = pathSegments.get(0);
        String assetType = pathSegments.get(1);
        String assetName = pathSegments.get(2);
        if (!("/" + assetPath + "/")
                .equals(AndroidProtocolHandlerJni.get().getAndroidResourcePath())) {
            Log.e(
                    TAG,
                    "Resource path does not start with "
                            + AndroidProtocolHandlerJni.get().getAndroidResourcePath()
                            + ": "
                            + uri);
            return null;
        }
        // Drop the file extension.
        assetName = assetName.split("\\.")[0];
        try {
            int fieldId = getFieldId(assetType, assetName);
            int valueType = getValueType(fieldId);
            if (valueType == TypedValue.TYPE_STRING) {
                return getOriginalApplicationContext(ContextUtils.getApplicationContext())
                        .getResources()
                        .openRawResource(fieldId);
            } else {
                Log.e(TAG, "Asset not of type string: " + uri);
                return null;
            }
        } catch (ClassNotFoundException e) {
            Log.e(TAG, "Unable to open resource URL: " + uri, e);
            return null;
        } catch (NoSuchFieldException e) {
            Log.e(TAG, "Unable to open resource URL: " + uri, e);
            return null;
        } catch (IllegalAccessException e) {
            Log.e(TAG, "Unable to open resource URL: " + uri, e);
            return null;
        }
    }

    private static InputStream openAsset(Uri uri) {
        assert uri.getScheme().equals(FILE_SCHEME);
        assert uri.getPath() != null;
        assert uri.getPath().startsWith(AndroidProtocolHandlerJni.get().getAndroidAssetPath());
        String path =
                uri.getPath()
                        .replaceFirst(AndroidProtocolHandlerJni.get().getAndroidAssetPath(), "");
        try {
            AssetManager assets =
                    getOriginalApplicationContext(ContextUtils.getApplicationContext()).getAssets();
            return assets.open(path, AssetManager.ACCESS_STREAMING);
        } catch (IOException e) {
            Log.e(TAG, "Unable to open asset URL: " + uri);
            return null;
        }
    }

    private static InputStream openContent(Uri uri) {
        assert uri.getScheme().equals(CONTENT_SCHEME);
        try {
            return ContextUtils.getApplicationContext().getContentResolver().openInputStream(uri);
        } catch (Exception e) {
            Log.e(TAG, "Unable to open content URL: " + uri);
            return null;
        }
    }

    /**
     * Determine the mime type for an Android resource.
     *
     * @param stream  The opened input stream which to examine.
     * @param url     The url from which the stream was opened.
     * @return The mime type or null if the type is unknown.
     */
    @CalledByNative
    public static String getMimeType(InputStream stream, GURL url) {
        Uri uri = verifyUrl(url);
        if (uri == null) {
            return null;
        }
        try {
            String path = uri.getPath();
            // The content URL type can be queried directly.
            if (uri.getScheme().equals(CONTENT_SCHEME)) {
                return ContextUtils.getApplicationContext().getContentResolver().getType(uri);
                // Asset files may have a known extension.
            } else if (uri.getScheme().equals(FILE_SCHEME)
                    && path.startsWith(AndroidProtocolHandlerJni.get().getAndroidAssetPath())) {
                String mimeType = URLConnection.guessContentTypeFromName(path);

                // If the OS didn't find anything we fall back to Chromium's implementation which
                // should be far more reliable since we can control this.
                // See crbug.com/1019528
                if (mimeType == null) {
                    mimeType = AndroidProtocolHandlerJni.get().getWellKnownMimeType(path);
                }

                if (mimeType != null) {
                    return mimeType;
                }
            }
        } catch (Exception ex) {
            Log.e(TAG, "Unable to get mime type" + url);
            return null;
        }
        // Fall back to sniffing the type from the stream.
        try {
            return URLConnection.guessContentTypeFromStream(stream);
        } catch (IOException e) {
            return null;
        }
    }

    /**
     * Make sure the given GURL is correctly formed and parse it into a Uri.
     *
     * @return a Uri instance, or null if the URL was invalid.
     */
    private static Uri verifyUrl(GURL url) {
        if (url == null) return null;
        if (url.isEmpty()) return null;
        // Never null. parse() doesn't actually parse or verify anything.
        Uri uri = Uri.parse(url.getSpec());
        String path = uri.getPath();
        if (path == null || path.isEmpty() || path.equals("/")) {
            Log.e(TAG, "URL does not have a path: " + url);
            return null;
        }
        return uri;
    }

    @NativeMethods
    interface Natives {
        String getAndroidAssetPath();

        String getAndroidResourcePath();

        String getWellKnownMimeType(String path);
    }
}
