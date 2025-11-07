// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.tabs.TabStripCollection;

/**
 * Forwards save requests for a specific collection to storage. This enables the ability to drive
 * collection updates through Java, instead of Native-level code.
 */
@NullMarked
@JNINamespace("tabs")
public class CollectionSaveForwarder implements Destroyable {
    private long mNativePtr;

    private CollectionSaveForwarder(long nativePtr) {
        mNativePtr = nativePtr;
    }

    /**
     * @param profile The profile associated with the collection.
     * @param groupId Represents the group to forward save requests for.
     * @param collection The parent of the group represented by `groupId`.
     */
    public static CollectionSaveForwarder createForTabGroup(
            Profile profile, Token groupId, TabStripCollection collection) {
        long nativePtr =
                CollectionSaveForwarderJni.get().createForTabGroup(profile, groupId, collection);
        return new CollectionSaveForwarder(nativePtr);
    }

    @Override
    public void destroy() {
        assert mNativePtr != 0;
        CollectionSaveForwarderJni.get().destroy(mNativePtr);
        mNativePtr = 0;
    }

    /** Saves the collection metadata to storage. */
    public void savePayload() {
        assert mNativePtr != 0;
        CollectionSaveForwarderJni.get().savePayload(mNativePtr);
    }

    @NativeMethods
    interface Natives {
        long createForTabGroup(
                @JniType("Profile*") Profile profile,
                @JniType("base::Token") Token groupId,
                @JniType("tabs::TabStripCollection*") TabStripCollection collection);

        void destroy(long nativeCollectionSaveForwarderAndroid);

        void savePayload(long nativeCollectionSaveForwarderAndroid);
    }
}
