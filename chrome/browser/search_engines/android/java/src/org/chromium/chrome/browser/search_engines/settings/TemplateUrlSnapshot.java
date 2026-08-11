// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.url.GURL;

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
}
