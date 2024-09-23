// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.text.TextUtils;

import androidx.browser.trusted.sharing.ShareData;

import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.browserservices.intents.WebApkShareTarget;
import org.chromium.net.MimeTypeFilter;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Computes data for Post Share Target. */
public class WebApkShareTargetUtil {
    // A class containing data required to generate a share target post request.
    protected static class PostData {
        public boolean isMultipartEncoding;
        public ArrayList<String> names;
        public ArrayList<Boolean> isValueFileUri;
        public ArrayList<String> values;
        public ArrayList<String> filenames;
        public ArrayList<String> types;

        public PostData(boolean isMultipartEncoding) {
            this.isMultipartEncoding = isMultipartEncoding;
            names = new ArrayList<>();
            isValueFileUri = new ArrayList<>();
            values = new ArrayList<>();
            filenames = new ArrayList<>();
            types = new ArrayList<>();
        }

        private void addPlainText(String name, String value) {
            names.add(name);
            isValueFileUri.add(false);
            values.add(value);
            filenames.add("");
            types.add("text/plain");
        }

        private void add(
                String name, String value, boolean isValueAFileUri, String fileName, String type) {
            names.add(name);
            values.add(value);
            isValueFileUri.add(isValueAFileUri);
            filenames.add(fileName);
            types.add(type);
        }
    }

    private static String getFileTypeFromContentUri(Uri uri) {
        return ContextUtils.getApplicationContext().getContentResolver().getType(uri);
    }

    private static String getFileNameFromContentUri(Uri uri) {
        if (uri.getScheme().equals("content")) {
            try (Cursor cursor =
                    ContextUtils.getApplicationContext()
                            .getContentResolver()
                            .query(uri, null, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    String result =
                            cursor.getString(
                                    cursor.getColumnIndexOrThrow(OpenableColumns.DISPLAY_NAME));
                    if (result != null) {
                        return result;
                    }
                }
            }
        }

        return uri.getPath();
    }

    public static String[] decodeJsonStringArray(String encodedJsonArray) {
        if (encodedJsonArray == null) {
            return null;
        }

        try {
            JSONArray jsonArray = new JSONArray(encodedJsonArray);
            String[] originalData = new String[jsonArray.length()];
            for (int i = 0; i < jsonArray.length(); i++) {
                originalData[i] = jsonArray.getString(i);
            }
            return originalData;
        } catch (JSONException e) {
        }
        return null;
    }

    public static String[][] decodeJsonAccepts(String encodedAcceptsArray) {
        if (encodedAcceptsArray == null) {
            return null;
        }
        try {
            JSONArray jsonArray = new JSONArray(encodedAcceptsArray);
            String[][] originalData = new String[jsonArray.length()][];
            for (int i = 0; i < jsonArray.length(); i++) {
                String[] childArr = new String[jsonArray.getJSONArray(i).length()];
                for (int j = 0; j < childArr.length; j++) {
                    childArr[j] = jsonArray.getJSONArray(i).getString(j);
                }
                originalData[i] = childArr;
            }
            return originalData;
        } catch (JSONException e) {
        }

        return null;
    }

    /**
     * Given a list of share target params file names, and the mime types each file name can
     * accept, returns the first share target params file name which accepts the passed-in file URI.
     */
    private static String findFormFieldToShareFile(
            Uri fileUri,
            String fileType,
            String[] shareTargetParamsFileNames,
            String[][] shareTargetParamsFileAccepts) {
        if (shareTargetParamsFileNames == null
                || shareTargetParamsFileAccepts == null
                || shareTargetParamsFileNames.length != shareTargetParamsFileAccepts.length) {
            return null;
        }
        for (int i = 0; i < shareTargetParamsFileNames.length; i++) {
            String[] mimeTypeList = shareTargetParamsFileAccepts[i];
            MimeTypeFilter mimeTypeFilter = new MimeTypeFilter(Arrays.asList(mimeTypeList), false);
            if (mimeTypeFilter.accept(fileUri, fileType)) {
                return shareTargetParamsFileNames[i];
            }
        }
        return null;
    }

    protected static void addFilesToMultipartPostData(
            PostData postData,
            String fallbackNameForPlainTextFile,
            String[] shareTargetParamsFileNames,
            String[][] shareTargetParamsFileAccepts,
            List<Uri> shareFiles) {
        if (shareFiles == null) {
            return;
        }

        for (Uri fileUri : shareFiles) {
            String fileType;
            String fileName;

            fileType = getFileTypeFromContentUri(fileUri);
            fileName = getFileNameFromContentUri(fileUri);

            if (fileType == null || fileName == null) {
                continue;
            }

            String fieldName =
                    findFormFieldToShareFile(
                            fileUri,
                            fileType,
                            shareTargetParamsFileNames,
                            shareTargetParamsFileAccepts);

            if (fieldName != null) {
                postData.add(
                        fieldName,
                        fileUri.toString(),
                        /* isValueAFileUri= */ true,
                        fileName,
                        fileType);
            } else if (fallbackNameForPlainTextFile != null && fileType.equals("text/plain")) {
                postData.add(
                        fallbackNameForPlainTextFile, fileUri.toString(), true, "", "text/plain");
                // we should only add one text file as a fake text selection
                fallbackNameForPlainTextFile = null;
            }
        }
    }

    /**
     * If a WebAPK has a share target parameter file name that receives sharing of text files, adds
     * the share text selection as if it's a text file.
     */
    private static void tryAddShareTextAsFakeFile(
            PostData postData,
            String[] shareTargetParamsFileNames,
            String[][] shareTargetParamsFileAccepts,
            String shareText) {
        if (TextUtils.isEmpty(shareText)) {
            return;
        }

        String fieldName =
                findFormFieldToShareFile(
                        null,
                        "text/plain",
                        shareTargetParamsFileNames,
                        shareTargetParamsFileAccepts);
        if (fieldName != null) {
            postData.add(
                    fieldName, shareText, /* isValueAFileUri= */ false, "shared.txt", "text/plain");
        }
    }

    protected static PostData computePostData(WebApkShareTarget shareTarget, ShareData shareData) {
        if (shareTarget == null || !shareTarget.isShareMethodPost() || shareData == null) {
            return null;
        }

        PostData postData = new PostData(shareTarget.isShareEncTypeMultipart());

        if (!TextUtils.isEmpty(shareTarget.getParamTitle())
                && !TextUtils.isEmpty(shareData.title)) {
            postData.addPlainText(shareTarget.getParamTitle(), shareData.title);
        }

        if (!TextUtils.isEmpty(shareTarget.getParamText()) && !TextUtils.isEmpty(shareData.text)) {
            postData.addPlainText(shareTarget.getParamText(), shareData.text);
        }

        if (!postData.isMultipartEncoding) {
            return postData;
        }

        // When a WebAPK doesn't expect a shared text selection, but receives one (because Android
        // intent filters don't distinguish between text selections and text files), we send the
        // text selection as if it's a text file.
        if (TextUtils.isEmpty(shareTarget.getParamText()) && !TextUtils.isEmpty(shareData.text)) {
            tryAddShareTextAsFakeFile(
                    postData,
                    shareTarget.getFileNames(),
                    shareTarget.getFileAccepts(),
                    shareData.text);
        }

        boolean enableAddingFileAsFakePlainText =
                !TextUtils.isEmpty(shareTarget.getParamText()) && TextUtils.isEmpty(shareData.text);

        // We allow adding a file as fake shared text only when shared text is absent, because the
        // web page expects a single value (not an array) in the "param text" field.
        addFilesToMultipartPostData(
                postData,
                enableAddingFileAsFakePlainText ? shareTarget.getParamText() : null,
                shareTarget.getFileNames(),
                shareTarget.getFileAccepts(),
                shareData.uris);

        return postData;
    }
}
