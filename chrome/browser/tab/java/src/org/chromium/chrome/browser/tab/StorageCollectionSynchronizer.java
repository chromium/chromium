// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.tabs.TabStripCollection;

/** Synchronizes the state of a {@link TabStripCollection} with storage. */
@NullMarked
@JNINamespace("tabs")
public class StorageCollectionSynchronizer implements Destroyable {
    @JniType("::tabs::StorageCollectionSynchronizerAndroid")
    interface NativeStorageCollectionSynchronizerAndroid extends JniTypeToken {}

    private final JniUniquePtr<NativeStorageCollectionSynchronizerAndroid> mNativePtr;

    /**
     * @param profile The profile associated with the collection.
     * @param collection The {@link TabStripCollection} to track.
     */
    public StorageCollectionSynchronizer(Profile profile, JniPtr<TabStripCollection> collection) {
        mNativePtr = StorageCollectionSynchronizerJni.get().init(profile, collection);
    }

    @Override
    public void destroy() {
        mNativePtr.destroy();
    }

    /**
     * Uses a StorageRestoreOrchestratorFactory to build and register a StorageRestoreOrchestrator
     * to receive updates to the TabStripCollection.
     */
    public void consumeRestoreOrchestratorFactory(StorageRestoreOrchestratorFactory factory) {
        StorageCollectionSynchronizerJni.get()
                .consumeRestoreOrchestratorFactory(mNativePtr, factory);
    }

    /**
     * Uses a CollectionStorageObserverFactory to build and register a CollectionStorageObserver to
     * receive updates to the TabStripCollection.
     */
    public void consumeCollectionObserverFactory(CollectionStorageObserverFactory factory) {
        StorageCollectionSynchronizerJni.get()
                .consumeCollectionObserverFactory(mNativePtr, factory);
    }

    /**
     * Saves a structural snapshot of the collection tree to storage.
     *
     * @param clearUnusedNodes Whether to prune unreferenced nodes in storage.
     * @param callback Callback to run once the snapshot has been committed to storage.
     */
    public void saveStructureSnapshot(boolean clearUnusedNodes, Runnable callback) {
        StorageCollectionSynchronizerJni.get()
                .saveStructureSnapshot(mNativePtr, clearUnusedNodes, callback);
    }

    /** Flushes any pending structural snapshot to storage immediately if one is scheduled. */
    public void flushPendingStructureSnapshot() {
        StorageCollectionSynchronizerJni.get().flushPendingStructureSnapshot(mNativePtr);
    }

    /** Cancels the restoration process. */
    public void cancelRestore() {
        StorageCollectionSynchronizerJni.get().cancelRestore(mNativePtr);
    }

    @NativeMethods
    interface Natives {
        JniUniquePtr<NativeStorageCollectionSynchronizerAndroid> init(
                @JniType("Profile*") Profile profile, JniPtr<TabStripCollection> collection);

        /** Native implementation of {@link #saveStructureSnapshot}. */
        void saveStructureSnapshot(
                JniPtr<NativeStorageCollectionSynchronizerAndroid> self,
                boolean clearUnusedNodes,
                @JniType("base::OnceClosure") Runnable callback);

        void flushPendingStructureSnapshot(JniPtr<NativeStorageCollectionSynchronizerAndroid> self);

        void cancelRestore(JniPtr<NativeStorageCollectionSynchronizerAndroid> self);

        void consumeRestoreOrchestratorFactory(
                JniPtr<NativeStorageCollectionSynchronizerAndroid> self,
                StorageRestoreOrchestratorFactory factory);

        void consumeCollectionObserverFactory(
                JniPtr<NativeStorageCollectionSynchronizerAndroid> self,
                CollectionStorageObserverFactory factory);
    }
}
