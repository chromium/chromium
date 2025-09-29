// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

/** Saves Java-accessible tab data for use in C++. */
@JNINamespace("tabs")
@NullMarked
public class TabStoragePackager {
    private final long mNativeTabStoragePackager;

    private TabStoragePackager(long nativeTabStoragePackager) {
        mNativeTabStoragePackager = nativeTabStoragePackager;
    }

    @CalledByNative
    private static TabStoragePackager create(long nativeTabStoragePackager) {
        return new TabStoragePackager(nativeTabStoragePackager);
    }
}
