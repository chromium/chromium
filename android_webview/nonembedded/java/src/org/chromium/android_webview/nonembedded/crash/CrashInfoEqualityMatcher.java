// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded.crash;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;

import java.util.Locale;
import java.util.Objects;

/** Matcher class to assert equality of two {@link CrashInfo} objects in tests. */
public class CrashInfoEqualityMatcher extends BaseMatcher<CrashInfo> {
    private static final String MISMATCH_STRING_FORMAT = "%s: found<%s>, expected<%s>";

    private final CrashInfo mCrashInfo;

    CrashInfoEqualityMatcher(CrashInfo crashInfo) {
        mCrashInfo = crashInfo;
    }

    @Override
    public boolean matches(Object o) {
        return matchWithMessage(o) == null;
    }

    @Override
    public void describeTo(Description description) {
        description.appendText("CrashInfo objects don't match");
    }

    @Override
    public void describeMismatch(Object item, Description description) {
        String mismatchMessage = matchWithMessage(item);
        if (mismatchMessage != null) description.appendText(mismatchMessage);
    }

    /**
     * Matches the given {@code Object} with the {@link CrashInfo} object of this class
     * and return mismtach string.
     *
     * @return message describes the mismatched field or {@code null} if the two objects
     *         matched.
     */
    private String matchWithMessage(Object o) {
        if (o == mCrashInfo) {
            return null;
        }
        if (o == null) {
            return "Item is null";
        }
        if (o.getClass() != mCrashInfo.getClass()) {
            return String.format(
                    Locale.US,
                    MISMATCH_STRING_FORMAT,
                    "class",
                    o.getClass(),
                    mCrashInfo.getClass());
        }

        CrashInfo c = (CrashInfo) o;
        StringBuilder builder = new StringBuilder();

        if (!Objects.equals(mCrashInfo.uploadState, c.uploadState)) {
            builder.append(
                    String.format(
                            Locale.US,
                            MISMATCH_STRING_FORMAT,
                            "uploadState",
                            c.uploadState,
                            mCrashInfo.uploadState));
        }
        if (!Objects.equals(mCrashInfo.localId, c.localId)) {
            builder.append(
                    String.format(
                            Locale.US,
                            MISMATCH_STRING_FORMAT,
                            "localId",
                            c.localId,
                            mCrashInfo.localId));
        }
        if (!Objects.equals(
                mCrashInfo.getCrashKey(CrashInfo.APP_PACKAGE_NAME_KEY),
                c.getCrashKey(CrashInfo.APP_PACKAGE_NAME_KEY))) {
            builder.append(
                    String.format(
                            Locale.US,
                            MISMATCH_STRING_FORMAT,
                            "appPackageName",
                            c.getCrashKey(CrashInfo.APP_PACKAGE_NAME_KEY),
                            mCrashInfo.getCrashKey(CrashInfo.APP_PACKAGE_NAME_KEY)));
        }
        if (!Objects.equals(mCrashInfo.uploadId, c.uploadId)) {
            builder.append(
                    String.format(
                            Locale.US,
                            MISMATCH_STRING_FORMAT,
                            "uploadId",
                            c.uploadId,
                            mCrashInfo.uploadId));
        }
        if (mCrashInfo.uploadTime != c.uploadTime) {
            builder.append(
                    String.format(
                            Locale.US,
                            MISMATCH_STRING_FORMAT,
                            "uploadTime",
                            c.uploadTime,
                            mCrashInfo.uploadTime));
        }
        if (mCrashInfo.captureTime != c.captureTime) {
            builder.append(
                    String.format(
                            Locale.US,
                            MISMATCH_STRING_FORMAT,
                            "captureTime",
                            c.captureTime,
                            mCrashInfo.captureTime));
        }

        if (mCrashInfo.isHidden != c.isHidden) {
            builder.append(
                    String.format(
                            Locale.US,
                            MISMATCH_STRING_FORMAT,
                            "isHidden",
                            c.isHidden,
                            mCrashInfo.isHidden));
        }
        // empty means a match
        return builder.length() == 0 ? null : builder.toString();
    }

    /** Create an equality {@link org.hamcrest.Matcher} for the given {@link CrashInfo object}. */
    public static CrashInfoEqualityMatcher equalsTo(CrashInfo c) {
        return new CrashInfoEqualityMatcher(c);
    }
}
