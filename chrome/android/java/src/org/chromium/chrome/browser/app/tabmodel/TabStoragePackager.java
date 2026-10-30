// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.chrome.browser.app.tabmodel.CustomTabsTabModelOrchestrator.getCustomTabsWindowTag;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.ARCHIVED_WINDOW_TAG;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_TASK_ID;

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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.tabs.TabStripCollection;

import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import java.util.function.Supplier;

/** Saves Java-accessible data for use in C++. */
@JNINamespace("tabs")
@NullMarked
public class TabStoragePackager {
    private final long mNativeTabStoragePackager;
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

    private TabStoragePackager(long nativeTabStoragePackager) {
        mNativeTabStoragePackager = nativeTabStoragePackager;
    }

    @CalledByNative
    private static TabStoragePackager create(long nativeTabStoragePackager) {
        return new TabStoragePackager(nativeTabStoragePackager);
    }

    @CalledByNative
    public static void fetchTabStorageMetadata(
            @Nullable @JniType("const TabAndroid*") Tab tab, long metadataPtr) {
        if (tab == null) return;
        TabStoragePackagerJni.get()
                .onTabStorageMetadataFetched(
                        metadataPtr,
                        tab.getTimestampMillis(),
                        tab.getThemeColor(),
                        tab.getLastNavigationCommittedTimestampMillis(),
                        tab.getTabHasSensitiveContent(),
                        TabAssociatedApp.getAppId(tab));
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
        TabModelInfo info = getArchivedModelInfo(profile, collection);
        if (info != null) return info;
        info = getWindowScopedModelInfo(collection);
        if (info != null) return info;
        info = getCustomTabModelInfo(collection);
        return info;
    }

    private @Nullable TabModelInfo getCustomTabModelInfo(JniPtr<TabStripCollection> collection) {
        Collection<TabModelSelector> selectors =
                TabWindowManagerSingleton.getInstance().getCustomTabsTabModelSelectors();
        TabModel tabModel = null;
        TabModelSelector selector = null;
        for (TabModelSelector currentSelector : selectors) {
            tabModel = currentSelector.getTabModelForTabStripCollection(collection);
            if (tabModel != null) {
                selector = currentSelector;
                break;
            }
        }
        if (tabModel == null || selector == null) return null;

        int taskId = TabWindowManagerSingleton.getInstance().getTaskIdForCustomTab(selector);
        if (taskId == INVALID_TASK_ID) return null;

        TabModelInfo info =
                TabModelInfo.createForCustomTabModel(
                        taskId,
                        tabModel.isOffTheRecord(),
                        (Supplier<@Nullable Tab>) tabModel.getCurrentTabSupplier());
        JniPtr<TabStripCollection> modelCollection = tabModel.getTabStripCollection();
        assert modelCollection != null;
        mTabModelInfoMap.put(modelCollection, info);
        configureRemoveFromCacheOnDestroy(tabModel, modelCollection);
        return info;
    }

    private @Nullable TabModelInfo getWindowScopedModelInfo(JniPtr<TabStripCollection> collection) {
        TabModel tabModel = null;
        TabModelSelector selector = null;
        Collection<TabModelSelector> selectors =
                TabWindowManagerSingleton.getInstance().getAllTabModelSelectors();
        for (TabModelSelector currentSelector : selectors) {
            tabModel = currentSelector.getTabModelForTabStripCollection(collection);
            if (tabModel != null) {
                selector = currentSelector;
                break;
            }
        }
        if (tabModel == null || selector == null) return null;

        @WindowId
        int windowId = TabWindowManagerSingleton.getInstance().getWindowIdForSelector(selector);
        if (windowId == TabWindowManager.INVALID_WINDOW_ID) return null;

        TabModelInfo info =
                TabModelInfo.createForWindowScopedModel(
                        windowId,
                        tabModel.isOffTheRecord(),
                        (Supplier<@Nullable Tab>) tabModel.getCurrentTabSupplier());
        JniPtr<TabStripCollection> modelCollection = tabModel.getTabStripCollection();
        assert modelCollection != null;
        mTabModelInfoMap.put(modelCollection, info);
        configureRemoveFromCacheOnDestroy(tabModel, modelCollection);
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

        assert archivedCollection != null;
        TabModelInfo info = TabModelInfo.createForArchivedModel();
        mTabModelInfoMap.put(archivedCollection, info);
        configureRemoveFromCacheOnDestroy(tabModel, archivedCollection);
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
                        mNativeTabStoragePackager,
                        info.windowTag,
                        info.tabModelType,
                        info.activeTabSupplier.get());
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

    @NativeMethods
    interface Natives {
        void onTabStorageMetadataFetched(
                long metadataPtr,
                long timestampMillis,
                int themeColor,
                long lastNavigationCommittedTimestampMillis,
                boolean tabHasSensitiveContent,
                @Nullable @JniType("std::optional<std::string>") String openerAppId);

        long consolidateTabStripCollectionData(
                long nativeTabStoragePackagerAndroid,
                @JniType("std::string") String windowTag,
                @TabModelType int tabModelType,
                @JniType("TabAndroid*") @Nullable Tab activeTab);
    }
}
