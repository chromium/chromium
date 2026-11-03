// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.List;

/** This class allows Java code to get and clear the list of recently closed entries. */
@JNINamespace("recent_tabs")
@NullMarked
public class RecentlyClosedBridge implements RecentlyClosedTabManager {
    @JniType("::recent_tabs::RecentlyClosedTabsBridge")
    public interface NativeRecentlyClosedTabsBridge extends JniTypeToken {}

    private @Nullable JniUniquePtr<NativeRecentlyClosedTabsBridge> mNativeBridge;
    private final TabModelSelector mTabModelSelector;

    private @Nullable Runnable mEntriesUpdatedRunnable;

    @CalledByNative
    private static void addTabToEntries(List<RecentlyClosedEntry> entries, RecentlyClosedTab tab) {
        entries.add(tab);
    }

    @CalledByNative
    private static void addGroupToEntries(
            List<RecentlyClosedEntry> entries,
            int id,
            long groupTimestamp,
            @JniType("std::u16string") String groupTitle,
            @TabGroupColorId int groupColor,
            @JniType("std::vector") List<RecentlyClosedTab> tabs) {
        RecentlyClosedGroup group =
                new RecentlyClosedGroup(id, groupTimestamp, groupTitle, groupColor);
        group.getTabs().addAll(tabs);
        entries.add(group);
    }

    @CalledByNative
    private static void addBulkEventToEntries(
            List<RecentlyClosedEntry> entries,
            int id,
            long eventTimestamp,
            @JniType("std::vector<std::optional<base::Token>>") Token[] tabGroupIds,
            @JniType("std::vector<const std::u16string*>") String[] groupTitles,
            @JniType("std::vector") List<RecentlyClosedTab> tabs) {
        RecentlyClosedBulkEvent event = new RecentlyClosedBulkEvent(id, eventTimestamp);

        assert tabGroupIds.length == groupTitles.length;
        for (int i = 0; i < tabGroupIds.length; i++) {
            event.getTabGroupIdToTitleMap().put(tabGroupIds[i], groupTitles[i]);
        }

        event.getTabs().addAll(tabs);
        entries.add(event);
    }

    @CalledByNative
    private void restoreTabGroup(
            @JniType("TabModel*") TabModel tabModel,
            @JniType("std::u16string") String title,
            int color,
            @JniType("std::vector") int[] tabIds) {
        if (tabIds.length == 0) return;

        assert mTabModelSelector.getModel(tabModel.isIncognito()) == tabModel;

        // TODO(b/336589861): Use savedTabGroupId to reassociate this tab group with a sync entity.

        int destinationId = tabIds[0];
        Tab tab = tabModel.getTabById(destinationId);
        assert tab != null;
        if (tabIds.length == 1) {
            tabModel.createSingleTabGroup(tab);
        } else {
            for (int id : tabIds) {
                if (id == destinationId) continue;

                tabModel.mergeTabsToGroup(id, destinationId);
            }
        }

        Token tabGroupId = tab.getTabGroupId();
        assert tabGroupId != null;
        tabModel.setTabGroupColor(tabGroupId, color);

        if (title == null || title.isEmpty()) return;

        tabModel.setTabGroupTitle(tabGroupId, title);
    }

    /**
     * Initializes this class with the given profile.
     *
     * @param profile The {@link Profile} whose recently closed tabs will be queried.
     * @param tabModelSelector The {@link TabModelSelector} to use to get {@link TabModel}s.
     */
    public RecentlyClosedBridge(Profile profile, TabModelSelector tabModelSelector) {
        mNativeBridge = RecentlyClosedBridgeJni.get().init(this, profile);
        mTabModelSelector = tabModelSelector;
    }

    @Override
    public void destroy() {
        assert mNativeBridge != null;
        mNativeBridge.destroy();
        mNativeBridge = null;
        mEntriesUpdatedRunnable = null;
    }

    @Override
    public void setEntriesUpdatedRunnable(@Nullable Runnable runnable) {
        mEntriesUpdatedRunnable = runnable;
    }

    @Override
    public @Nullable List<RecentlyClosedEntry> getRecentlyClosedEntries(int maxEntryCount) {
        assert mNativeBridge != null;
        List<RecentlyClosedEntry> entries = new ArrayList<>();
        boolean received =
                RecentlyClosedBridgeJni.get()
                        .getRecentlyClosedEntries(mNativeBridge, entries, maxEntryCount);
        return received ? entries : null;
    }

    /**
     * Commits all pending tab closures in the regular tab model. Restoring a tab can synchronously
     * commit pending tab group closures, which calls back into TabRestoreService's
     * CreateHistoricalGroup. To prevent concurrent modification issues in TabRestoreService, we
     * commit all pending closures before starting restoration.
     */
    private void commitTabClosuresInRegularModel() {
        TabModel regularModel = mTabModelSelector.getModel(/* incognito= */ false);
        if (regularModel != null) {
            regularModel.commitAllTabClosures();
        }
    }

    @Override
    public boolean openRecentlyClosedTab(
            TabModel tabModel, RecentlyClosedTab recentTab, int windowOpenDisposition) {
        assert mNativeBridge != null;
        assert mTabModelSelector.getModel(tabModel.isIncognito()) == tabModel;
        commitTabClosuresInRegularModel();
        return RecentlyClosedBridgeJni.get()
                .openRecentlyClosedTab(
                        mNativeBridge, tabModel, recentTab.getSessionId(), windowOpenDisposition);
    }

    @Override
    public boolean openRecentlyClosedEntry(TabModel tabModel, RecentlyClosedEntry recentEntry) {
        assert mNativeBridge != null;
        assert mTabModelSelector.getModel(tabModel.isIncognitoBranded()) == tabModel
                && recentEntry instanceof SessionRecentlyClosedEntry;
        commitTabClosuresInRegularModel();
        SessionRecentlyClosedEntry sessionRecentEntry = (SessionRecentlyClosedEntry) recentEntry;
        return RecentlyClosedBridgeJni.get()
                .openRecentlyClosedEntry(
                        mNativeBridge, tabModel, sessionRecentEntry.getSessionId());
    }

    /**
     * Restores a previously saved entry of closed tabs for an undo operation.
     *
     * <p>Unlike standard restore methods, this does not commit pending tab closures because doing
     * so would dismiss other undo snackbars. Must be called on the UI thread.
     *
     * @param tabModel The {@link TabModel} to restore the tabs into.
     * @param entryId The id of the TabRestoreService entry to restore, as returned when the tabs
     *     were saved.
     * @param tabIds The old id of each closed tab. This and the following arrays are parallel; an
     *     index into them is the tab's placement index. Restored tabs are matched to placement
     *     indices by their old id.
     * @param indices The index at which each tab should be restored in the tab model.
     * @param pinned The pinned state to restore for each tab.
     * @return The new tab id per placement index (-1 if not restored), or an empty array if the
     *     entry does not exist or there is no service.
     */
    public int[] restoreForUndo(
            TabModel tabModel,
            @TabRestoreEntryId int entryId,
            int[] tabIds,
            int[] indices,
            boolean[] pinned) {
        ThreadUtils.assertOnUiThread();
        assert mNativeBridge != null;
        assert mTabModelSelector.getModel(tabModel.isIncognitoBranded()) == tabModel;
        assert tabIds.length == indices.length;
        assert tabIds.length == pinned.length;
        return RecentlyClosedBridgeJni.get()
                .restoreForUndo(mNativeBridge, tabModel, entryId, tabIds, indices, pinned);
    }

    @Override
    public void openMostRecentlyClosedEntry(TabModel tabModel) {
        assert mNativeBridge != null;
        assert mTabModelSelector.getModel(tabModel.isIncognito()) == tabModel;
        commitTabClosuresInRegularModel();
        RecentlyClosedBridgeJni.get().openMostRecentlyClosedEntry(mNativeBridge, tabModel);
    }

    @Override
    public void clearRecentlyClosedEntries() {
        assert mNativeBridge != null;
        RecentlyClosedBridgeJni.get().clearRecentlyClosedEntries(mNativeBridge);
    }

    @Override
    public void clearLeastRecentlyUsedClosedEntries(int numToRemove) {
        if (numToRemove == 0) return;
        assert mNativeBridge != null;
        RecentlyClosedBridgeJni.get()
                .clearLeastRecentlyUsedClosedEntries(mNativeBridge, numToRemove);
    }

    /** This method will be called every time the list of recently closed tabs is updated. */
    @CalledByNative
    private void onUpdated() {
        if (mEntriesUpdatedRunnable != null) mEntriesUpdatedRunnable.run();
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        JniUniquePtr<NativeRecentlyClosedTabsBridge> init(
                RecentlyClosedBridge self, @JniType("Profile*") Profile profile);

        boolean getRecentlyClosedEntries(
                JniPtr<NativeRecentlyClosedTabsBridge> self,
                List<RecentlyClosedEntry> entries,
                int maxEntryCount);

        boolean openRecentlyClosedTab(
                JniPtr<NativeRecentlyClosedTabsBridge> self,
                @JniType("TabModel*") TabModel tabModel,
                int tabSessionId,
                int windowOpenDisposition);

        boolean openRecentlyClosedEntry(
                JniPtr<NativeRecentlyClosedTabsBridge> self,
                @JniType("TabModel*") TabModel tabModel,
                int sessionId);

        @JniType("std::vector<int32_t>")
        int[] restoreForUndo(
                JniPtr<NativeRecentlyClosedTabsBridge> self,
                @JniType("TabModel*") TabModel tabModel,
                int entryId,
                @JniType("std::vector<int32_t>") int[] tabIds,
                @JniType("std::vector<int32_t>") int[] indices,
                @JniType("std::vector<bool>") boolean[] pinned);

        boolean openMostRecentlyClosedEntry(
                JniPtr<NativeRecentlyClosedTabsBridge> self,
                @JniType("TabModel*") TabModel tabModel);

        void clearRecentlyClosedEntries(JniPtr<NativeRecentlyClosedTabsBridge> self);

        void clearLeastRecentlyUsedClosedEntries(
                JniPtr<NativeRecentlyClosedTabsBridge> self, int numToRemove);
    }
}
