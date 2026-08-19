// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.chrome.browser.tabpersistence.TabStateFileManager.FLATBUFFER_PREFIX;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/** Key abstraction for identifying storage and preloading entries within {@link TabCache}. */
@NullMarked
public final class TabCacheKey {
    private static final String REGULAR_SUFFIX = "_regular";
    private static final String INCOGNITO_SUFFIX = "_incognito";

    private final String mTag;
    private final boolean mIsIncognito;

    /**
     * Constructs a {@link TabCacheKey} with the specified tag identifier and incognito status.
     *
     * @param tag The tag identifier.
     * @param isIncognito Whether the entry corresponds to an incognito model.
     */
    public TabCacheKey(String tag, boolean isIncognito) {
        mTag = tag;
        mIsIncognito = isIncognito;
    }

    /** Returns the tag identifier. */
    public String getTag() {
        return mTag;
    }

    /** Returns whether the entry is for an incognito model. */
    public boolean isIncognito() {
        return mIsIncognito;
    }

    /** Computes the flatbuffer file name for this entry. */
    public String getFileName() {
        String suffix = mIsIncognito ? INCOGNITO_SUFFIX : REGULAR_SUFFIX;
        return FLATBUFFER_PREFIX + mTag + suffix;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof TabCacheKey other)) return false;
        return mIsIncognito == other.mIsIncognito && Objects.equals(mTag, other.mTag);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mTag, mIsIncognito);
    }

    @Override
    public String toString() {
        return "TabCacheKey{" + mTag + ", incognito=" + mIsIncognito + "}";
    }
}
