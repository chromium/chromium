// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.metrics;

import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.base.FileUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/**
 * A helper class to handle categorizing and detecting file extensions.  This is primarily used for
 * downloaded content, so the file types and categorizations are based on typically downloaded file
 * types.
 */
public final class FileExtensions {
    /** A list of enumerated file extensions. */
    // These statics are used for UMA logging. Please update the AndroidDownloadFilterType enum in
    // histograms.xml if these change.
    @IntDef({Type.OTHER, Type.APK, Type.CSV, Type.DOC, Type.DOCX, Type.EXE, Type.PDF, Type.PPT,
            Type.PPTX, Type.PSD, Type.RTF, Type.TXT, Type.XLS, Type.XLSX, Type.ZIP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int OTHER = 0;
        int APK = 1;
        int CSV = 2;
        int DOC = 3;
        int DOCX = 4;
        int EXE = 5;
        int PDF = 6;
        int PPT = 7;
        int PPTX = 8;
        int PSD = 9;
        int RTF = 10;
        int TXT = 11;
        int XLS = 12;
        int XLSX = 13;
        int ZIP = 14;
        int NUM_ENTRIES = 15;
    }

    private static final Map<String, Integer> EXTENSIONS_MAP;
    static {
        Map<String, Integer> extensions = new HashMap<>();
        extensions.put("apk", Type.APK);
        extensions.put("csv", Type.CSV);
        extensions.put("doc", Type.DOC);
        extensions.put("docx", Type.DOCX);
        extensions.put("exe", Type.EXE);
        extensions.put("pdf", Type.PDF);
        extensions.put("ppt", Type.PPT);
        extensions.put("pptx", Type.PPTX);
        extensions.put("psd", Type.PSD);
        extensions.put("rtf", Type.RTF);
        extensions.put("txt", Type.TXT);
        extensions.put("xls", Type.XLS);
        extensions.put("xlsx", Type.XLSX);
        extensions.put("zip", Type.ZIP);

        EXTENSIONS_MAP = Collections.unmodifiableMap(extensions);
    }

    /**
     * Attempts to retrieve the file extension of {@code filePath} and categorize it into one of the
     * {@link FileExtensions#Type}s.
     * @param filePath The file path to attempt to query an extension from.
     * @return         The corresponding {@link FileExtensions#Type} categorized extension type.
     */
    public static @Type int getExtension(String filePath) {
        String extension = FileUtils.getExtension(filePath);
        if (TextUtils.isEmpty(extension)) return Type.OTHER;
        extension = extension.toLowerCase(Locale.getDefault());
        if (!EXTENSIONS_MAP.containsKey(extension)) return Type.OTHER;
        return EXTENSIONS_MAP.get(extension);
    }

    private FileExtensions() {}
}