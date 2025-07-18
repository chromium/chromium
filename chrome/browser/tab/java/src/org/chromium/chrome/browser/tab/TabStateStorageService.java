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
    private final long mNativeTabStateStorageService;

    private TabStateStorageService(long nativeTabStateStorageService) {
        mNativeTabStateStorageService = nativeTabStateStorageService;
    }

    @CalledByNative
    private static TabStateStorageService create(long nativeTabStateStorageService) {
        return new TabStateStorageService(nativeTabStateStorageService);
    }

    /**
     * Saves the tab state to persistent storage. This approach takes raw fields instead of an
     * object.
     *
     * @param id The id of the tab.
     * @param parentCollectionId The id of the parent.
     * @param position A sortable field to decide the order of tabs in a given parent.
     * @param parentTabId The tab id of the tab that spawned this tab, optional.
     * @param rootId If the tab is part of a tab group, the owner tab id.
     * @param timestampMillis The last time it was shown.
     * @param webContentsStateBuffer Holds serialized web contents data.
     * @param openerAppId If associated with another app, its id. Optional.
     * @param themeColor The toolbar color specified by the page. Optional.
     * @param launchTypeAtCreation How the tab was created.
     * @param userAgent What user agent should be passed in the HTTP requests.
     * @param lastNavigationCommittedTimestampMillis The time the last navigation was made.
     * @param tabGroupId The group id if the tab is in a group. Optional.
     * @param tabHasSensitiveContent If there is sensitive content.
     * @param isPinned Whether the tab is pinned.
     */
    public void saveTabData(
            int id,
            int parentCollectionId,
            String position,
            int parentTabId,
            int rootId,
            long timestampMillis,
            @Nullable ByteBuffer webContentsStateBuffer,
            String openerAppId,
            int themeColor,
            int launchTypeAtCreation,
            @TabUserAgent int userAgent,
            long lastNavigationCommittedTimestampMillis,
            @Nullable Token tabGroupId,
            boolean tabHasSensitiveContent,
            boolean isPinned) {
        TabStateStorageServiceJni.get()
                .saveTab(
                        mNativeTabStateStorageService,
                        id,
                        parentCollectionId,
                        position,
                        parentTabId,
                        rootId,
                        timestampMillis,
                        webContentsStateBuffer,
                        openerAppId,
                        themeColor,
                        launchTypeAtCreation,
                        userAgent,
                        lastNavigationCommittedTimestampMillis,
                        tabGroupId,
                        tabHasSensitiveContent,
                        isPinned);
    }

    /**
     * Loads all the tabs into TabState objects and asynchronously runs the given callback.
     * TODO(https://crbug.com/427254267): Add tab id/sort order to this.
     * TODO(https://crbug.com/430996004): Scope to a given window.
     *
     * @param callback Run with loaded tab data.
     */
    public void loadAllTabs(Callback<TabState[]> callback) {
        TabStateStorageServiceJni.get().loadAllTabs(mNativeTabStateStorageService, callback);
    }

    @CalledByNative
    public static TabState createTabState(
            int parentTabId,
            int rootId,
            long timestampMillis,
            @Nullable ByteBuffer webContentsStateBuffer,
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
            tabState.contentsState = new WebContentsState(webContentsStateBuffer);
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
        void saveTab(
                long nativeTabStateStorageService,
                int id,
                int parentCollectionId,
                @JniType("std::string") String position,
                int parentTabId,
                int rootId,
                long timestampMillis,
                @Nullable ByteBuffer webContentsStateBuffer,
                @Nullable @JniType("std::string") String openerAppId,
                int themeColor,
                int launchTypeAtCreation,
                int userAgent,
                long lastNavigationCommittedTimestampMillis,
                @Nullable Token tabGroupId,
                boolean tabHasSensitiveContent,
                boolean isPinned);

        void loadAllTabs(long nativeTabStateStorageService, Callback<TabState[]> callback);
    }
}
