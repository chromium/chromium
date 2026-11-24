// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.chrome.browser.app.tabmodel.CustomTabsTabModelOrchestrator.getCustomTabsWindowTag;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.ARCHIVED_WINDOW_TAG;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_TASK_ID;

import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.tabs.TabStripCollection;

import java.nio.ByteBuffer;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import java.util.function.Supplier;

/** Saves Java-accessible data for use in C++. */
@JNINamespace("tabs")
@NullMarked
public class TabStoragePackager {
    private final Map<JniPtr<TabStripCollection>, TabModelInfo> mTabModelInfoMap = new HashMap<>();

    /** A data object representing a {@link TabModel} and its associated window tag. */
    private static class TabModelInfo {
        public final String windowTag;
        public final @TabModelType int tabModelType;
        public final boolean isOffTheRecord;
        public final Supplier<@Nullable Tab> activeTabSupplier;

        /**
         * @param windowTag The window tag the {@link TabModel} is associated with.
         * @param tabModelType The type of tab model being saved.
         * @param isOffTheRecord If the tab model is off the record.
         * @param activeTabSupplier The supplier of the active tab in the tab model.
         */
        TabModelInfo(
                String windowTag,
                boolean isOffTheRecord,
                @TabModelType int tabModelType,
                Supplier<@Nullable Tab> activeTabSupplier) {
            this.windowTag = windowTag;
            this.isOffTheRecord = isOffTheRecord;
            this.tabModelType = tabModelType;
            this.activeTabSupplier = activeTabSupplier;
        }

        /**
         * @param windowId The {@link WindowId} the {@link TabModel} is associated with.
         * @param isOffTheRecord Whether the tab model is off the record.
         * @param activeTabSupplier The supplier of the active tab in the tab model.
         */
        public static TabModelInfo createForWindowScopedModel(
                @WindowId int windowId,
                boolean isOffTheRecord,
                Supplier<@Nullable Tab> activeTabSupplier) {
            return new TabModelInfo(
                    String.valueOf(windowId),
                    isOffTheRecord,
                    isOffTheRecord ? TabModelType.INCOGNITO : TabModelType.REGULAR,
                    activeTabSupplier);
        }

        /**
         * @param taskId The task ID for the activity the {@link TabModel} is associated with.
         * @param isOffTheRecord Whether the tab model is off the record.
         * @param activeTabSupplier The supplier of the active tab in the tab model.
         */
        public static TabModelInfo createForCustomTabModel(
                int taskId, boolean isOffTheRecord, Supplier<@Nullable Tab> activeTabSupplier) {
            return new TabModelInfo(
                    getCustomTabsWindowTag(taskId),
                    isOffTheRecord,
                    TabModelType.CUSTOM,
                    activeTabSupplier);
        }

        /**
         * @param tabModel The {@link TabModel} associated with the {@link
         *     ArchivedTabModelOrchestrator}.
         */
        public static TabModelInfo createForArchivedModel() {
            return new TabModelInfo(
                    ARCHIVED_WINDOW_TAG,
                    /* isOffTheRecord= */ false,
                    TabModelType.ARCHIVED,
                    /* activeTabSupplier= */ SupplierUtils.ofNull());
        }
    }

    @VisibleForTesting
    TabStoragePackager() {}

    @CalledByNative
    private static TabStoragePackager create() {
        return new TabStoragePackager();
    }

    @CalledByNative
    public static void fetchTabStorageMetadata(
            @Nullable @JniType("const TabAndroid*") Tab tab, long metadataPtr) {
        if (tab == null) return;
        ByteBuffer buffer = null;
        int version = WebContentsState.INVALID_BUFFER_VERSION;
        boolean hasPendingLoadParams = tab.getPendingLoadParams() != null;
        if (hasPendingLoadParams) {
            WebContentsState state = TabStateExtractor.getWebContentsState(tab);
            if (state != null
                    && state.version() != WebContentsState.INVALID_BUFFER_VERSION
                    && state.buffer().remaining() > 0) {
                buffer = state.buffer();
                version = state.version();
            }
        }
        TabStoragePackagerJni.get()
                .onTabStorageMetadataFetched(
                        metadataPtr,
                        tab.getTimestampMillis(),
                        tab.getThemeColor(),
                        tab.getLastNavigationCommittedTimestampMillis(),
                        tab.getTabHasSensitiveContent(),
                        TabAssociatedApp.getAppId(tab),
                        hasPendingLoadParams,
                        buffer,
                        version);
    }

    private @Nullable TabModelInfo getTabModelInfo(
            Profile profile, JniPtr<TabStripCollection> collection) {
        if (mTabModelInfoMap.containsKey(collection)) {
            return mTabModelInfoMap.get(collection);
        }

        return resolveTabModelInfo(profile, collection);
    }

    private @Nullable TabModelInfo resolveTabModelInfo(
            Profile profile, JniPtr<TabStripCollection> collection) {
        Profile originalProfile = resolveOriginalProfile(profile);
        TabModelInfo info = getArchivedModelInfo(originalProfile, collection);
        if (info != null) return info;
        info = getWindowScopedModelInfo(originalProfile, collection);
        if (info != null) return info;
        info = getCustomTabModelInfo(originalProfile, collection);
        return info;
    }

    private @Nullable TabModelInfo getCustomTabModelInfo(
            Profile originalProfile, JniPtr<TabStripCollection> collection) {
        Collection<TabModelSelector> selectors =
                TabWindowManagerSingleton.getInstance().getCustomTabsTabModelSelectors();
        Pair<TabModelSelector, TabModel> match =
                findMatchingSelectorAndModel(selectors, originalProfile, collection);
        if (match == null) return null;

        TabModelSelector selector = match.first;
        TabModel tabModel = match.second;
        int taskId = TabWindowManagerSingleton.getInstance().getTaskIdForCustomTab(selector);
        if (taskId == INVALID_TASK_ID) return null;

        TabModelInfo info =
                TabModelInfo.createForCustomTabModel(
                        taskId,
                        tabModel.isOffTheRecord(),
                        (Supplier<@Nullable Tab>) tabModel.getCurrentTabSupplier());
        cacheTabModelInfo(tabModel, info);
        return info;
    }

    private @Nullable TabModelInfo getWindowScopedModelInfo(
            Profile originalProfile, JniPtr<TabStripCollection> collection) {
        Collection<TabModelSelector> selectors =
                TabWindowManagerSingleton.getInstance().getAllTabModelSelectors();
        Pair<TabModelSelector, TabModel> match =
                findMatchingSelectorAndModel(selectors, originalProfile, collection);
        if (match == null) return null;

        TabModelSelector selector = match.first;
        TabModel tabModel = match.second;
        @WindowId
        int windowId = TabWindowManagerSingleton.getInstance().getWindowIdForSelector(selector);
        if (windowId == TabWindowManager.INVALID_WINDOW_ID) return null;

        TabModelInfo info =
                TabModelInfo.createForWindowScopedModel(
                        windowId,
                        tabModel.isOffTheRecord(),
                        (Supplier<@Nullable Tab>) tabModel.getCurrentTabSupplier());
        cacheTabModelInfo(tabModel, info);
        return info;
    }

    private @Nullable TabModelInfo getArchivedModelInfo(
            Profile profile, JniPtr<TabStripCollection> collection) {
        ArchivedTabModelOrchestrator orchestrator =
                ArchivedTabModelOrchestrator.getIfInstantiatedForProfile(profile);
        if (orchestrator == null) return null;

        TabModel tabModel = orchestrator.getTabModel();
        if (tabModel == null) return null;

        JniPtr<TabStripCollection> archivedCollection = tabModel.getTabStripCollection();
        if (!Objects.equals(archivedCollection, collection)) return null;

        TabModelInfo info = TabModelInfo.createForArchivedModel();
        cacheTabModelInfo(tabModel, info);
        return info;
    }

    @CalledByNative
    public long packageTabStripCollection(
            @JniType("Profile*") Profile profile,
            @JniType("const tabs::TabStripCollection*") JniPtr<TabStripCollection> collection) {
        TabModelInfo info = getTabModelInfo(profile, collection);
        assert info != null;
        return TabStoragePackagerJni.get()
                .consolidateTabStripCollectionData(
                        info.windowTag, info.tabModelType, info.activeTabSupplier.get());
    }

    @CalledByNative
    public boolean isOffTheRecord(
            @JniType("Profile*") Profile profile,
            @JniType("const tabs::TabStripCollection*") JniPtr<TabStripCollection> collection) {
        TabModelInfo info = getTabModelInfo(profile, collection);
        assert info != null;
        return info.isOffTheRecord;
    }

    @CalledByNative
    public @JniType("std::string") String getWindowTag(
            @JniType("Profile*") Profile profile,
            @JniType("const tabs::TabStripCollection*") JniPtr<TabStripCollection> collection) {
        TabModelInfo info = getTabModelInfo(profile, collection);
        assert info != null;
        return info.windowTag;
    }

    private void cacheTabModelInfo(TabModel tabModel, TabModelInfo info) {
        JniPtr<TabStripCollection> modelCollection = tabModel.getTabStripCollection();
        assert modelCollection != null;
        mTabModelInfoMap.put(modelCollection, info);
        configureRemoveFromCacheOnDestroy(tabModel, modelCollection);
    }

    private void configureRemoveFromCacheOnDestroy(
            TabModel tabModel, JniPtr<TabStripCollection> collection) {
        tabModel.addObserver(
                new TabModelObserver() {
                    @Override
                    public void onDestroy() {
                        mTabModelInfoMap.remove(collection);
                        tabModel.removeObserver(this);
                    }
                });
    }

    private static @Nullable Pair<TabModelSelector, TabModel> findMatchingSelectorAndModel(
            Collection<TabModelSelector> selectors,
            Profile originalProfile,
            JniPtr<TabStripCollection> collection) {
        for (TabModelSelector selector : selectors) {
            if (!selectorMatchesProfile(selector, originalProfile)) continue;
            TabModel tabModel = selector.getTabModelForTabStripCollection(collection);
            if (tabModel != null) {
                return Pair.create(selector, tabModel);
            }
        }
        return null;
    }

    private static boolean selectorMatchesProfile(
            TabModelSelector selector, Profile originalProfile) {
        return Objects.equals(selector.getProfile(/* offTheRecord= */ false), originalProfile);
    }

    private static Profile resolveOriginalProfile(Profile profile) {
        return profile.isOffTheRecord() ? profile.getOriginalProfile() : profile;
    }

    @NativeMethods
    interface Natives {
        void onTabStorageMetadataFetched(
                long metadataPtr,
                long timestampMillis,
                int themeColor,
                long lastNavigationCommittedTimestampMillis,
                boolean tabHasSensitiveContent,
                @Nullable @JniType("std::optional<std::string>") String openerAppId,
                boolean hasPendingLoadParams,
                @Nullable ByteBuffer webContentsStateBuffer,
                int savedStateVersion);

        long consolidateTabStripCollectionData(
                @JniType("std::string") String windowTag,
                @TabModelType int tabModelType,
                @JniType("TabAndroid*") @Nullable Tab activeTab);
    }
}
