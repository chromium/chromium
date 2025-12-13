// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_import;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/** The JNI bridge for the DataImporterService. */
@NullMarked
public class DataImporterBridge {
    private long mNativeDataImporterBridge;

    /**
     * Creates a {@link DataImporterBridge}.
     *
     * @param profile {@link Profile} The profile for which to import user data.
     */
    public DataImporterBridge(Profile profile) {
        assert (profile != null);
        mNativeDataImporterBridge = DataImporterBridgeJni.get().init(profile);
    }

    /** Destroys this instance so no further calls can be executed. */
    public void destroy() {
        if (mNativeDataImporterBridge != 0) {
            DataImporterBridgeJni.get().destroy(mNativeDataImporterBridge);
            mNativeDataImporterBridge = 0;
        }
    }

    /**
     * Imports bookmarks from the specified fd (file descriptor). This assumes ownership of the fd,
     * meaning it must not be used in Java anymore; the native side will close it once done.
     */
    public void importBookmarks(int ownedFd, Callback<Integer> callback) {
        DataImporterBridgeJni.get().importBookmarks(mNativeDataImporterBridge, ownedFd, callback);
    }

    /**
     * Imports reading list entries from the specified fd (file descriptor). This assumes ownership
     * of the fd, meaning it must not be used in Java anymore; the native side will close it once
     * done.
     */
    public void importReadingList(int ownedFd, Callback<Integer> callback) {
        DataImporterBridgeJni.get().importReadingList(mNativeDataImporterBridge, ownedFd, callback);
    }

    /**
     * Imports history entries from the specified fd (file descriptor). This assumes ownership of
     * the fd, meaning it must not be used in Java anymore; the native side will close it once done.
     */
    public void importHistory(int ownedFd, Callback<Integer> callback) {
        DataImporterBridgeJni.get().importHistory(mNativeDataImporterBridge, ownedFd, callback);
    }

    @NativeMethods
    interface Natives {
        long init(@JniType("Profile*") @Nullable Profile profile);

        void destroy(long nativeDataImporterBridge);

        void importBookmarks(
                long nativeDataImporterBridge, int ownedFd, Callback<Integer> callback);

        void importReadingList(
                long nativeDataImporterBridge, int ownedFd, Callback<Integer> callback);

        void importHistory(long nativeDataImporterBridge, int ownedFd, Callback<Integer> callback);
    }
}
