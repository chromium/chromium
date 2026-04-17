// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

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

        public LoadedTabState(@TabId int tabId, TabState tabState) {
            this.tabId = tabId;
            this.tabState = tabState;
        }
    }

    /** Simple data container for a warning which occurred during the load process. */
    public static class StorageLoadWarning {
        public final @StorageLoadWarningCode int code;
        public final String message;

        public StorageLoadWarning(@StorageLoadWarningCode int code, String message) {
            this.code = code;
            this.message = message;
        }
    }

    private long mNativePtr;
    private final LoadedTabState[] mLoadedTabStates;
    private final TabGroupCollectionData[] mGroupsData;
    private final int mActiveTabIndex;
    private final StorageLoadWarning[] mWarnings;

    private StorageLoadedData(
            long nativePtr,
            LoadedTabState[] loadedTabStates,
            TabGroupCollectionData[] groupsData,
            int activeTabIndex,
            StorageLoadWarning[] warnings) {
        mNativePtr = nativePtr;
        mLoadedTabStates = loadedTabStates;
        mGroupsData = groupsData;
        mActiveTabIndex = activeTabIndex;
        mWarnings = warnings;
    }

    @Override
    public void destroy() {
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
            long nativePtr,
            @JniType("std::vector<tabs_pb::TabState>") LoadedTabState[] loadedTabStates,
            @JniType("std::vector<tabs::TabGroupCollectionDataAndroid*>")
                    TabGroupCollectionData[] groups,
            int activeTabIndex,
            @JniType("std::vector<tabs::StorageLoadingContext::Warning>")
                    StorageLoadWarning[] warnings) {
        return new StorageLoadedData(nativePtr, loadedTabStates, groups, activeTabIndex, warnings);
    }

    @CalledByNative
    public static LoadedTabState createLoadedTabState(@TabId int tabId, TabState tabState) {
        return new LoadedTabState(tabId, tabState);
    }

    @CalledByNative
    public static StorageLoadWarning createStorageLoadWarning(
            @StorageLoadWarningCode int code, @JniType("std::string") String message) {
        return new StorageLoadWarning(code, message);
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
            boolean isPinned,
            @Nullable @JniType("std::string") String url) {
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

        if (url != null) {
            GURL gurl = new GURL(url);
            if (gurl.isValid()) tabState.url = gurl;
        }

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

    /** Returns the index of the active tab or -1 if one is not set. */
    public int getActiveTabIndex() {
        return mActiveTabIndex;
    }

    /** Returns the warnings which occurred during the load process. */
    public StorageLoadWarning[] getWarnings() {
        return mWarnings;
    }

    @NativeMethods
    interface Natives {
        void destroy(long nativeStorageLoadedDataAndroid);
    }
}
