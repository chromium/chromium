// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.text.TextUtils;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * A class holding constants and convenience methods about filters and their corresponding
 * resources.
 */
public class DownloadFilter {
    // These statics are used for UMA logging. Please update the AndroidDownloadFilterType enum in
    // histograms.xml if these change.
    @IntDef({Type.ALL, Type.PAGE, Type.VIDEO, Type.AUDIO, Type.IMAGE, Type.DOCUMENT, Type.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int ALL = 0;
        int PAGE = 1;
        int VIDEO = 2;
        int AUDIO = 3;
        int IMAGE = 4;
        int DOCUMENT = 5;
        int OTHER = 6;
        int NUM_ENTRIES = 7;
    }

    private static final String MIMETYPE_VIDEO = "video";
    private static final String MIMETYPE_AUDIO = "audio";
    private static final String MIMETYPE_IMAGE = "image";
    private static final String MIMETYPE_DOCUMENT = "text";

    /** Identifies the type of file represented by the given MIME type string. */
    public static @Type int fromMimeType(String mimeType) {
        if (TextUtils.isEmpty(mimeType)) return Type.OTHER;

        Integer type = filterForSpecialMimeTypes(mimeType);
        if (type != null) return type;

        String[] pieces = mimeType.toLowerCase(Locale.getDefault()).split("/");
        if (pieces.length != 2) return Type.OTHER;

        if (MIMETYPE_VIDEO.equals(pieces[0])) {
            return Type.VIDEO;
        } else if (MIMETYPE_AUDIO.equals(pieces[0])) {
            return Type.AUDIO;
        } else if (MIMETYPE_IMAGE.equals(pieces[0])) {
            return Type.IMAGE;
        } else if (MIMETYPE_DOCUMENT.equals(pieces[0])) {
            return Type.DOCUMENT;
        } else {
            return Type.OTHER;
        }
    }

    private static Integer filterForSpecialMimeTypes(String mimeType) {
        if (mimeType.equals("application/ogg")) return Type.AUDIO;
        return null;
    }
}
