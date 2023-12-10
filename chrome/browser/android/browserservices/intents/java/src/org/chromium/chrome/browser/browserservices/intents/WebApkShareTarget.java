// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import androidx.annotation.Nullable;

import java.util.Arrays;

/** Stores information about the WebAPK's share intent handlers. */
public class WebApkShareTarget {
    private static final int ACTION_INDEX = 0;
    private static final int PARAM_TITLE_INDEX = 1;
    private static final int PARAM_TEXT_INDEX = 2;
    private String[] mData;
    private boolean mIsShareMethodPost;
    private boolean mIsShareEncTypeMultipart;
    private String[] mFileNames;
    private String[][] mFileAccepts;

    public WebApkShareTarget(
            String action,
            String paramTitle,
            String paramText,
            boolean isMethodPost,
            boolean isEncTypeMultipart,
            String[] fileNames,
            String[][] fileAccepts) {
        mData = new String[3];
        mData[ACTION_INDEX] = replaceNullWithEmpty(action);
        mData[PARAM_TITLE_INDEX] = replaceNullWithEmpty(paramTitle);
        mData[PARAM_TEXT_INDEX] = replaceNullWithEmpty(paramText);
        mIsShareMethodPost = isMethodPost;
        mIsShareEncTypeMultipart = isEncTypeMultipart;

        mFileNames = fileNames != null ? fileNames : new String[0];
        mFileAccepts = fileAccepts != null ? fileAccepts : new String[0][];
    }

    public static boolean equals(@Nullable WebApkShareTarget s1, @Nullable WebApkShareTarget s2) {
        if (s1 == null) {
            return (s2 == null);
        }
        if (s2 == null) {
            return false;
        }

        return Arrays.equals(s1.mData, s2.mData)
                && s1.mIsShareMethodPost == s2.mIsShareMethodPost
                && s1.mIsShareEncTypeMultipart == s2.mIsShareEncTypeMultipart
                && Arrays.equals(s1.mFileNames, s2.mFileNames)
                && Arrays.deepEquals(s1.mFileAccepts, s2.mFileAccepts);
    }

    public String getAction() {
        return mData[ACTION_INDEX];
    }

    public String getParamTitle() {
        return mData[PARAM_TITLE_INDEX];
    }

    public String getParamText() {
        return mData[PARAM_TEXT_INDEX];
    }

    public boolean isShareMethodPost() {
        return mIsShareMethodPost;
    }

    public boolean isShareEncTypeMultipart() {
        return mIsShareEncTypeMultipart;
    }

    public String[] getFileNames() {
        return mFileNames;
    }

    public String[][] getFileAccepts() {
        return mFileAccepts;
    }

    /** Returns the value if it is non-null. Returns an empty string otherwise. */
    private static String replaceNullWithEmpty(String value) {
        return (value == null) ? "" : value;
    }
}
