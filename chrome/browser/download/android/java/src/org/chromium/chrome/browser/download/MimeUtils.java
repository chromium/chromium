// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.text.TextUtils;
import android.webkit.MimeTypeMap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/** Utility class for MIME type related operations. */
public class MimeUtils {
    // MIME types for OMA downloads.
    public static final String OMA_DOWNLOAD_DESCRIPTOR_MIME = "application/vnd.oma.dd+xml";
    public static final String OMA_DRM_MESSAGE_MIME = "application/vnd.oma.drm.message";
    public static final String OMA_DRM_CONTENT_MIME = "application/vnd.oma.drm.content";
    public static final String OMA_DRM_RIGHTS_MIME = "application/vnd.oma.drm.rights+wbxml";

    private static final String UNKNOWN_MIME_TYPE = "application/unknown";

    // Mime types that Android can't handle when tries to open the file. Chrome may deduct a better
    // mime type based on file extension.
    private static final HashSet<String> GENERIC_MIME_TYPES =
            new HashSet<String>(
                    Arrays.asList(
                            "text/plain",
                            "application/octet-stream",
                            "binary/octet-stream",
                            "octet/stream",
                            "application/download",
                            "application/force-download",
                            "application/unknown"));

    // Set will be more expensive to initialize, so use an ArrayList here.
    private static final List<String> MIME_TYPES_TO_OPEN =
            new ArrayList<String>(
                    Arrays.asList(
                            MimeUtils.OMA_DOWNLOAD_DESCRIPTOR_MIME,
                            "application/pdf",
                            "application/x-x509-ca-cert",
                            "application/x-x509-user-cert",
                            "application/x-x509-server-cert",
                            "application/x-pkcs12",
                            "application/application/x-pem-file",
                            "application/pkix-cert",
                            "application/x-wifi-config"));

    /**
     * If the given MIME type is null, or one of the "generic" types (text/plain or
     * application/octet-stream) map it to a type that Android can deal with. If the given type is
     * not generic, return it unchanged.
     *
     * @param mimeType MIME type provided by the server.
     * @param url URL of the data being loaded.
     * @param filename file name obtained from content disposition header
     * @return The MIME type that should be used for this data.
     */
    @CalledByNative
    public static @JniType("std::string") String remapGenericMimeType(
            @JniType("std::string") String mimeType,
            @JniType("std::string") String url,
            @JniType("std::string") String filename) {
        if (TextUtils.isEmpty(mimeType)) mimeType = UNKNOWN_MIME_TYPE;
        if (GENERIC_MIME_TYPES.contains(mimeType)) {
            String extension = getFileExtension(url, filename);
            String newMimeType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
            if (newMimeType != null) {
                mimeType = newMimeType;
            } else if (extension.equals("dm")) {
                mimeType = OMA_DRM_MESSAGE_MIME;
            } else if (extension.equals("dd")) {
                mimeType = OMA_DOWNLOAD_DESCRIPTOR_MIME;
            }
        }
        return mimeType;
    }

    /**
     * Retrieve the file extension from a given file name or url.
     *
     * @param url URL to extract the extension.
     * @param filename File name to extract the extension.
     * @return If extension can be extracted from file name, use that. Or otherwise, use the
     *         extension extracted from the url.
     */
    static String getFileExtension(String url, String filename) {
        if (!TextUtils.isEmpty(filename)) {
            int index = filename.lastIndexOf(".");
            if (index > 0) return filename.substring(index + 1);
        }
        return MimeTypeMap.getFileExtensionFromUrl(url);
    }

    /**
     * Helper method to find apps that can open PDF file.
     *
     * @return A list of ResolveInfo that can open the PDF type.
     */
    public static List<ResolveInfo> getPdfIntentHandlers() {
        Intent intent = new Intent(Intent.ACTION_VIEW);

        intent.setDataAndType(Uri.fromFile(new File("/empty.pdf")), "application/pdf");
        return PackageManagerUtils.queryIntentActivities(intent, 0);
    }

    /**
     * Helper method to get the app name for the first pdf viewer returned by
     * queryIntentActivities().
     *
     * @return App name.
     */
    public static String getDefaultPdfViewerName() {
        List<ResolveInfo> resolveInfos = getPdfIntentHandlers();
        if (resolveInfos.size() > 0) {
            return resolveInfos
                    .get(0)
                    .loadLabel(ContextUtils.getApplicationContext().getPackageManager())
                    .toString();
        }
        return null;
    }

    /**
     * Returns true if the download is for OMA download description file.
     *
     * @param mimeType The mime type of the download.
     * @return true if the downloaded is OMA download description, or false otherwise.
     */
    @CalledByNative
    public static boolean isOMADownloadDescription(@JniType("std::string") String mimeType) {
        return OMA_DOWNLOAD_DESCRIPTOR_MIME.equalsIgnoreCase(mimeType);
    }

    /**
     * Determines if the download should be immediately opened after downloading.
     *
     * @param mimeType The mime type of the download.
     * @return true if the downloaded content should be opened, or false otherwise.
     */
    @CalledByNative
    public static boolean canAutoOpenMimeType(@JniType("std::string") String mimeType) {
        return MIME_TYPES_TO_OPEN.contains(mimeType);
    }
}
