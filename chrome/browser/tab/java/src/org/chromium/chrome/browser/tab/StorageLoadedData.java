// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.JniOnceCallback;
import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.nio.ByteBuffer;

/**
 * Data container for data obtained from {@link TabStateStorageService}. Must be destroyed to
 * prevent memory leaks.
 */
@NullMarked
@JNINamespace("tabs")
public class StorageLoadedData implements Destroyable {
    /** Simple data container for a TabState and its corresponding creation callback. */
    public static class LoadedTabState {
        public final @TabId int tabId;
        public final TabState tabState;

        /** This must always be run or destroyed to avoid leaks. */
        public final JniOnceCallback<@Nullable Tab> onTabCreationCallback;

        public LoadedTabState(
                @TabId int tabId,
                TabState tabState,
                JniOnceCallback<@Nullable Tab> onTabCreationCallback) {
            this.tabId = tabId;
            this.tabState = tabState;
            this.onTabCreationCallback = onTabCreationCallback;
        }
    }

    private long mNativePtr;
    private final LoadedTabState[] mLoadedTabStates;
    private final TabGroupCollectionData[] mGroupsData;

    private StorageLoadedData(
            long nativePtr, LoadedTabState[] loadedTabStates, TabGroupCollectionData[] groupsData) {
        mNativePtr = nativePtr;
        mLoadedTabStates = loadedTabStates;
        mGroupsData = groupsData;
    }

    @Override
    public void destroy() {
        for (LoadedTabState loadedTabState : getLoadedTabStates()) {
            loadedTabState.onTabCreationCallback.destroy();
            // We don't destroy the contentsState here. Leave this to clients to clean.
        }

        for (TabGroupCollectionData groupData : getGroupsData()) {
            groupData.destroy();
        }

        assert mNativePtr != 0;
        StorageLoadedDataJni.get().destroy(mNativePtr);
        mNativePtr = 0;
    }

    @CalledByNative
    public long getNativePtr() {
        return mNativePtr;
    }

    @CalledByNative
    public static StorageLoadedData createData(
            long nativePtr, LoadedTabState[] loadedTabStates, TabGroupCollectionData[] groups) {
        return new StorageLoadedData(nativePtr, loadedTabStates, groups);
    }

    @CalledByNative
    public static LoadedTabState createLoadedTabState(
            @TabId int tabId,
            TabState tabState,
            JniOnceCallback<@Nullable Tab> onTabCreationCallback) {
        return new LoadedTabState(tabId, tabState, onTabCreationCallback);
    }

    @CalledByNative
    public static TabState createTabState(
            int parentTabId,
            int rootId,
            long timestampMillis,
            @Nullable ByteBuffer webContentsStateBuffer,
            int webContentsStateVersion,
            long webContentsStateStringPointer,
            @Nullable @JniType("std::string") String openerAppId,
            int themeColor,
            int launchTypeAtCreation,
            int userAgent,
            long lastNavigationCommittedTimestampMillis,
            @Nullable Token tabGroupId,
            boolean tabHasSensitiveContent,
            boolean isPinned) {
        TabState tabState = new TabState();
        tabState.parentId = parentTabId;
        tabState.rootId = rootId;
        tabState.timestampMillis = timestampMillis;
        if (webContentsStateBuffer != null) {
            assert webContentsStateStringPointer != 0;
            tabState.contentsState =
                    new WebContentsState(
                            webContentsStateBuffer,
                            webContentsStateVersion,
                            webContentsStateStringPointer);
        }
        tabState.openerAppId = openerAppId;
        tabState.themeColor = themeColor;
        tabState.tabLaunchTypeAtCreation = launchTypeAtCreation;
        tabState.userAgent = userAgent;
        tabState.lastNavigationCommittedTimestampMillis = lastNavigationCommittedTimestampMillis;
        tabState.tabGroupId = tabGroupId;
        tabState.tabHasSensitiveContent = tabHasSensitiveContent;
        tabState.isPinned = isPinned;
        return tabState;
    }

    /** Returns the loaded tab states. */
    public LoadedTabState[] getLoadedTabStates() {
        return mLoadedTabStates;
    }

    /** Returns the data for the tab groups. */
    public TabGroupCollectionData[] getGroupsData() {
        return mGroupsData;
    }

    @NativeMethods
    interface Natives {
        void destroy(long nativeStorageLoadedDataAndroid);
    }
}
