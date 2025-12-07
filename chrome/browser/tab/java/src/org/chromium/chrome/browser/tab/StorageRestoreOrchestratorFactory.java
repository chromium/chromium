// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.tabs.TabStripCollection;

/**
 * Java Factory for StorageRestoreOrchestrator. This class facilitates the restoration of a
 * TabStripCollection from storage.
 */
@NullMarked
@JNINamespace("tabs")
public class StorageRestoreOrchestratorFactory {
    public final Profile profile;
    public final TabStripCollection collection;
    public final StorageLoadedData loadedData;

    public StorageRestoreOrchestratorFactory(
            Profile profile, TabStripCollection collection, StorageLoadedData loadedData) {
        this.profile = profile;
        this.collection = collection;
        this.loadedData = loadedData;
    }

    @CalledByNative
    public static long build(StorageRestoreOrchestratorFactory factory) {
        return StorageRestoreOrchestratorFactoryJni.get()
                .build(factory.profile, factory.collection, factory.loadedData);
    }

    @NativeMethods
    interface Natives {
        long build(
                @JniType("Profile*") Profile profile,
                @JniType("tabs::TabStripCollection*") TabStripCollection collection,
                @JniType("StorageLoadedDataAndroid*") StorageLoadedData loadedData);
    }
}
