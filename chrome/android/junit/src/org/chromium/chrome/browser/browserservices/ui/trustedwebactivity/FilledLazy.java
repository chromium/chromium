// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.trustedwebactivity;

import dagger.Lazy;

/**
 * An implementation of {@link Lazy} that hold a pre-made value.
 *
 * @param <T> The type of the value the Lazy would hold.
 */
public class FilledLazy<T> implements Lazy<T> {
    private final T mContents;

    public FilledLazy(T contents) {
        mContents = contents;
    }

    @Override
    public T get() {
        return mContents;
    }
}
