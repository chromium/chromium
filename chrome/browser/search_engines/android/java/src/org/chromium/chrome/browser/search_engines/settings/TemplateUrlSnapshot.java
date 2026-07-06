// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.url.GURL;

import java.util.Locale;
import java.util.Objects;

/**
 * An immutable snapshot of a {@link TemplateUrl}'s properties, stashed on the Java heap to avoid
 * unsafe JNI calls to potentially stale native objects.
 */
@NullMarked
class TemplateUrlSnapshot {
    private final String mKeyword;
    private final String mShortName;
    private final boolean mIsPrepopulated;
    private final long mLastVisitedTime;
    private final long mId;
    private final GURL mFaviconUrl;
    private final @Nullable Bitmap mBuiltInIcon;

    public TemplateUrlSnapshot(TemplateUrl templateUrl) {
        mKeyword = templateUrl.getKeyword();
        mShortName = templateUrl.getShortName();
        mIsPrepopulated = templateUrl.getIsPrepopulated();
        mLastVisitedTime = templateUrl.getLastVisitedTime();
        mId = templateUrl.getId();
        mFaviconUrl = templateUrl.getFaviconURL();
        mBuiltInIcon = templateUrl.getBuiltInSearchEngineIcon();
    }

    /** Creates a snapshot from a {@link TemplateUrl}. */
    public static TemplateUrlSnapshot from(TemplateUrl templateUrl) {
        return new TemplateUrlSnapshot(templateUrl);
    }

    public String getKeyword() {
        return mKeyword;
    }

    public String getShortName() {
        return mShortName;
    }

    public boolean getIsPrepopulated() {
        return mIsPrepopulated;
    }

    public long getLastVisitedTime() {
        return mLastVisitedTime;
    }

    public long getId() {
        return mId;
    }

    public GURL getFaviconUrl() {
        return mFaviconUrl;
    }

    public @Nullable Bitmap getBuiltInIcon() {
        return mBuiltInIcon;
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) return true;
        if (!(o instanceof TemplateUrlSnapshot)) return false;
        TemplateUrlSnapshot that = (TemplateUrlSnapshot) o;
        return mIsPrepopulated == that.mIsPrepopulated
                && mLastVisitedTime == that.mLastVisitedTime
                && mId == that.mId
                && Objects.equals(mKeyword, that.mKeyword)
                && Objects.equals(mShortName, that.mShortName)
                && Objects.equals(mFaviconUrl, that.mFaviconUrl)
                && Objects.equals(mBuiltInIcon, that.mBuiltInIcon);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                mKeyword,
                mShortName,
                mIsPrepopulated,
                mLastVisitedTime,
                mId,
                mFaviconUrl,
                mBuiltInIcon);
    }

    @Override
    public String toString() {
        return String.format(
                Locale.US,
                "TemplateUrlSnapshot { name: %s, keyword: %s, prepopulated: %b, id: %d }",
                mShortName,
                mKeyword,
                mIsPrepopulated,
                mId);
    }
}
