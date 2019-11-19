// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * Bridge to interface with send_tab_to_self_model_observer_bridge which interacts with the
 * corresponding model observer. This is used by the SendTabToSelfInfobarController to listen
 * for model changes and show an infobar to the user.
 */
@JNINamespace("send_tab_to_self")
public class SendTabToSelfModelObserverBridge {
    private long mNativeModelObserverBridge;

    private final List<SendTabToSelfObserver> mObservers;

    /**
     * Java version of the observer that implementors can use to listen for model changes.
     */
    public abstract class SendTabToSelfObserver {
        /**
         * Corresponds to the EntriesAddedRemotely function in SendTabToSelfModelObserver.
         * @param entries Entries removed remotely.
         */
        public abstract void entriesAddedRemotely(List<SendTabToSelfEntry> entries);

        /**
         * Corresponds to the EntriesRemovedRemotely function in SendTabToSelfModelObserver.
         * @param guids Guids of the entries that were removed.
         */
        public abstract void entriesRemovedRemotely(List<String> guids);

        /**
         * Corresponds to the ModelLoaded function in SendTabToSelfModelObserver.
         * @return whether the model is loaded or not.
         */
        public abstract boolean modelLoaded();
    }

    /**
     * Constructs a new bridge for the profile provided.
     * @param profile Profile to construct the bridge for.
     */
    public SendTabToSelfModelObserverBridge(Profile profile) {
        Natives jni = SendTabToSelfModelObserverBridgeJni.get();
        mNativeModelObserverBridge = jni.init(this, profile);
        mObservers = new ArrayList<SendTabToSelfObserver>();
    }

    /** Destroys this instance so no further calls can be executed. */
    public void destroy() {
        if (mNativeModelObserverBridge != 0) {
            Natives jni = SendTabToSelfModelObserverBridgeJni.get();
            jni.destroy(mNativeModelObserverBridge);
            mNativeModelObserverBridge = 0;
        }
    }

    /**
     * Adds an observer to listen for model changes.
     * @param observer Observer to listen for model changes.
     */
    public void addObserver(SendTabToSelfObserver observer) {
        mObservers.add(observer);
    }

    /**
     * Removes an observer to no longer listen for model changes.
     * @param observer Observer to remove.
     */
    public void removeObserver(SendTabToSelfObserver observer) {
        mObservers.remove(observer);
    }

    /**
     * @return An empty array list to be populated by native code with SendTabToSelfEntries.
     */
    @CalledByNative
    private List<SendTabToSelfEntry> createEmptyJavaEntryList() {
        return new ArrayList<SendTabToSelfEntry>();
    }

    /**
     * Adds an entry to the list of entries. Called by native code.
     * @param entries List to add to.
     * @param entry Entry to add to the list.
     */
    @CalledByNative
    private void addToEntryList(List<SendTabToSelfEntry> entries, SendTabToSelfEntry entry) {
        entries.add(entry);
    }

    /**
     * @return An empty array list to be populated by native code with Guids (strings).
     */
    @CalledByNative
    private List<String> createEmptyJavaGuidList() {
        return new ArrayList<String>();
    }

    /**
     * Adds a guid to the list of Guids. Called by native code.
     * @param guids List to add to.
     * @param guid Guid to add to the list.
     */
    @CalledByNative
    private void addToGuidList(List<String> guids, String guid) {
        guids.add(guid);
    }

    /**
     * Called by native code in send_tab_to_self_model_observer when the model has a new entry.
     * @param newEntries The new entries pushed by the model.
     */
    @CalledByNative
    private void entriesAddedRemotely(List<SendTabToSelfEntry> newEntries) {
        for (SendTabToSelfObserver observer : mObservers) {
            observer.entriesAddedRemotely(newEntries);
        }
    }

    /**
     * Called by the native code in send_tab_to_self_model_observer when the model has a deletion
     * update.
     * @param guids Guids corresponding to the entries that were removed.
     */
    @CalledByNative
    private void entriesRemovedRemotely(List<String> guids) {
        for (SendTabToSelfObserver observer : mObservers) {
            observer.entriesRemovedRemotely(guids);
        }
    }
    /**
     * Called by the native code in send_tab_to_self_model_observer when the model is loaded.
     */
    @CalledByNative
    private void modelLoaded() {
        for (SendTabToSelfObserver observer : mObservers) {
            observer.modelLoaded();
        }
    }

    @NativeMethods
    interface Natives {
        long init(SendTabToSelfModelObserverBridge bridge, Profile profile);

        void destroy(long nativeSendTabToSelfModelObserverBridge);
    }
}
