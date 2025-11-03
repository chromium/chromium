// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.tabs.TabStripCollection;

/**
 * Java counterpart for the native StorageRestoreOrchestrator. This class facilitates the
 * restoration of a TabStripCollection from storage.
 */
@NullMarked
@JNINamespace("tabs")
public class StorageRestoreOrchestrator {
    private long mNativePtr;

    public StorageRestoreOrchestrator(
            Profile profile, TabStripCollection collection, StorageLoadedData loadedData) {
        mNativePtr = StorageRestoreOrchestratorJni.get().init(profile, collection, loadedData);
    }

    /** Saves all queued changes to storage. */
    public void save() {
        assert mNativePtr != 0;
        StorageRestoreOrchestratorJni.get().save(mNativePtr);
    }

    /** Destroys the native counterpart of this object. */
    public void destroy() {
        assert mNativePtr != 0;
        StorageRestoreOrchestratorJni.get().destroy(mNativePtr);
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        long init(
                @JniType("Profile*") Profile profile,
                @JniType("tabs::TabStripCollection*") TabStripCollection collection,
                @JniType("StorageLoadedDataAndroid*") StorageLoadedData loadedData);

        void save(long nativeStorageRestoreOrchestratorAndroid);

        void destroy(long nativeStorageRestoreOrchestratorAndroid);
    }
}
