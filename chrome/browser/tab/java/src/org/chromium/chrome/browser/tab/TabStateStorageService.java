// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.nio.ByteBuffer;

/** Java counterpart to keyed service in native that writes tab data to disk. */
@JNINamespace("tabs")
@NullMarked
public class TabStateStorageService {
    /** Simple data container for a TabState and its corresponding creation callback. */
    public static class LoadedTabState {
        public final @TabId int tabId;
        public final TabState tabState;
        public final Callback<@Nullable Tab> onTabCreationCallback;

        public LoadedTabState(
                @TabId int tabId,
                TabState tabState,
                Callback<@Nullable Tab> onTabCreationCallback) {
            this.tabId = tabId;
            this.tabState = tabState;
            this.onTabCreationCallback = onTabCreationCallback;
        }
    }

    private final long mNativeTabStateStorageService;

    private TabStateStorageService(long nativeTabStateStorageService) {
        mNativeTabStateStorageService = nativeTabStateStorageService;
    }

    @CalledByNative
    private static TabStateStorageService create(long nativeTabStateStorageService) {
        return new TabStateStorageService(nativeTabStateStorageService);
    }

    /**
     * Saves the tab state to persistent storage.
     *
     * @param tab The tab to save to storage.
     */
    public void saveTabData(Tab tab) {
        TabStateStorageServiceJni.get().save(mNativeTabStateStorageService, tab);
    }

    /**
     * Loads all the tabs into TabState objects and asynchronously runs the given callback.
     * TODO(https://crbug.com/427254267): Add tab id/sort order to this.
     * TODO(https://crbug.com/430996004): Scope to a given window.
     *
     * @param callback Run with loaded tab data.
     */
    public void loadAllTabs(Callback<LoadedTabState[]> callback) {
        TabStateStorageServiceJni.get().loadAllTabs(mNativeTabStateStorageService, callback);
    }

    @CalledByNative
    public static LoadedTabState createLoadedTabState(
            @TabId int tabId, TabState tabState, Callback<@Nullable Tab> onTabCreationCallback) {
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
        // TODO(skym): Handle id, parent_collection_id, position somehow.
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

    @NativeMethods
    interface Natives {
        void save(long nativeTabStateStorageServiceAndroid, @JniType("TabAndroid*") Tab tab);

        void loadAllTabs(
                long nativeTabStateStorageServiceAndroid, Callback<LoadedTabState[]> callback);
    }
}
