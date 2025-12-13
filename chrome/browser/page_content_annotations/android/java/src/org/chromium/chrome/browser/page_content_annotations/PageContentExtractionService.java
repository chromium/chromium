// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_content_annotations;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

@NullMarked
@JNINamespace("page_content_annotations")
public class PageContentExtractionService {
    private long mNativePageContentExtractionServiceAndroid;

    private PageContentExtractionService(long nativePtr) {
        mNativePageContentExtractionServiceAndroid = nativePtr;
    }

    /**
     * Get the list of all tab ids currently cached.
     *
     * @param callback The resulting list of tab ids.
     */
    public void getAllCachedTabIds(Callback<long[]> callback) {
        PageContentExtractionServiceJni.get()
                .getAllCachedTabIds(mNativePageContentExtractionServiceAndroid, callback);
    }

    @CalledByNative
    private static PageContentExtractionService create(long nativePtr) {
        return new PageContentExtractionService(nativePtr);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativePageContentExtractionServiceAndroid = 0;
    }

    @NativeMethods
    interface Natives {
        void getAllCachedTabIds(
                long nativePageContentExtractionServiceAndroid, Callback<long[]> callback);
    }
}
