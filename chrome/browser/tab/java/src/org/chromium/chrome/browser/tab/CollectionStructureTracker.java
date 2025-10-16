// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.tabs.TabStripCollection;

/** Synchronizes the state of a {@link TabStripCollection} with storage. */
@NullMarked
@JNINamespace("tabs")
public class CollectionStructureTracker implements Destroyable {
    private long mNativePtr;

    /**
     * @param profile The profile associated with the collection.
     * @param collection The {@link TabStripCollection} to track.
     */
    public CollectionStructureTracker(Profile profile, TabStripCollection collection) {
        mNativePtr = CollectionStructureTrackerJni.get().init(this, profile, collection);
    }

    @Override
    public void destroy() {
        assert mNativePtr != 0;
        CollectionStructureTrackerJni.get().destroy(mNativePtr);
        mNativePtr = 0;
    }

    /** Fully synchronizes the state of the collection and descendants with the storage layer. */
    public void fullSave() {
        assert mNativePtr != 0;
        CollectionStructureTrackerJni.get().fullSave(mNativePtr);
    }

    @NativeMethods
    interface Natives {
        long init(
                CollectionStructureTracker self,
                @JniType("Profile*") Profile profile,
                @JniType("tabs::TabStripCollection*") TabStripCollection collection);

        void fullSave(long nativeCollectionStructureTrackerAndroid);

        void destroy(long nativeCollectionStructureTrackerAndroid);
    }
}
