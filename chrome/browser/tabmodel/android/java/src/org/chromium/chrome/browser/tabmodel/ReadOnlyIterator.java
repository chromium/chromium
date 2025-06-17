// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Iterator;

/**
 * A wrapper for an {@link Iterator} that prevents modification of the underlying collection.
 *
 * @param <T> The type of elements returned by this iterator.
 */
@NullMarked
public class ReadOnlyIterator<T> implements Iterator<T> {
    private final Iterator<T> mIterator;

    /**
     * Constructs a read-only view of the specified iterator.
     *
     * @param iterator The iterator to be wrapped.
     */
    private ReadOnlyIterator(Iterator<T> iterator) {
        mIterator = iterator;
    }

    @Override
    public boolean hasNext() {
        return mIterator.hasNext();
    }

    @Override
    public @Nullable T next() {
        return mIterator.next();
    }

    @Override
    public void remove() {
        if (BuildConfig.ENABLE_ASSERTS) {
            throw new UnsupportedOperationException("Removal is not supported from this iterator");
        }
    }

    public static <T> Iterator<T> maybeCreate(Iterator<T> toWrap) {
        return BuildConfig.ENABLE_ASSERTS ? new ReadOnlyIterator<>(toWrap) : toWrap;
    }
}
