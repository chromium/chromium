// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import androidx.annotation.IntDef;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.CollectionUtil;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Creates historical entries in TabRestoreService. */
@JNINamespace("historical_tab_saver")
public class HistoricalTabSaverImpl implements HistoricalTabSaver {
    private static final List<String> UNSUPPORTED_SCHEMES =
            Arrays.asList(
                    UrlConstants.CHROME_SCHEME,
                    UrlConstants.CHROME_NATIVE_SCHEME,
                    ContentUrlConstants.ABOUT_SCHEME);

    private final List<Supplier<TabModel>> mSecondaryTabModelSuppliers = new ArrayList<>();
    private final TabModel mTabModel;

    private boolean mIgnoreUrlSchemesForTesting;

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({
        HistoricalSaverCloseType.TAB,
        HistoricalSaverCloseType.GROUP,
        HistoricalSaverCloseType.BULK,
        HistoricalSaverCloseType.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface HistoricalSaverCloseType {
        int TAB = 0;
        int GROUP = 1;
        int BULK = 2;
        int COUNT = 3;
    }

    /**
     * @param tabModel The model from which tabs are being saved.
     */
    public HistoricalTabSaverImpl(TabModel tabModel) {
        mTabModel = tabModel;
        mIgnoreUrlSchemesForTesting = false;
    }

    // HistoricalTabSaver implementation.

    @Override
    public void destroy() {
        mSecondaryTabModelSuppliers.clear();
    }

    @Override
    public void addSecodaryTabModelSupplier(Supplier<TabModel> tabModelSupplier) {
        mSecondaryTabModelSuppliers.add(tabModelSupplier);
    }

    @Override
    public void removeSecodaryTabModelSupplier(Supplier<TabModel> tabModelSupplier) {
        mSecondaryTabModelSuppliers.remove(tabModelSupplier);
    }

    @Override
    public void createHistoricalTab(Tab tab) {
        if (!shouldSave(tab)) return;

        createHistoricalTabInternal(tab);
    }

    @Override
    public void createHistoricalTabOrGroup(HistoricalEntry entry) {
        createHistoricalBulkClosure(Collections.singletonList(entry));
    }

    @Override
    public void createHistoricalBulkClosure(List<HistoricalEntry> entries) {
        // Filter out any invalid entire and tabs.
        List<HistoricalEntry> validEntries = getValidatedEntries(entries);
        if (validEntries.isEmpty()) return;

        // All tabs to be saved - one entry per tab.
        List<Tab> allTabs = new ArrayList<>();
        // Group IDs corresponding to each element of allTabs.
        List<Integer> perTabRootId = new ArrayList<>();

        // Distinct group IDs that will be saved - one per group.
        List<Integer> rootIds = new ArrayList<>();
        List<Token> tabGroupIds = new ArrayList<>();
        List<String> savedTabGroupIds = new ArrayList();
        // Titles corresponding to each element in rootIds.
        List<String> groupTitles = new ArrayList<>();
        // Colors corresponding to each element in rootIds.
        List<Integer> groupColors = new ArrayList<>();

        // Byte buffer associated with WebContentsState per tab by index.
        List<ByteBuffer> byteBuffers = new ArrayList<>();
        // Saved state version of WebContentsState per tab by index.
        List<Integer> savedStateVersions = new ArrayList<>();

        for (HistoricalEntry entry : validEntries) {
            if (entry.isSingleTab()) {
                WebContentsState tabWebContentsState = getWebContentsState(entry.getTabs().get(0));
                allTabs.add(entry.getTabs().get(0));
                perTabRootId.add(Tab.INVALID_TAB_ID);
                byteBuffers.add(tabWebContentsState.buffer());
                savedStateVersions.add(tabWebContentsState.version());
                continue;
            }

            rootIds.add(entry.getRootId());
            tabGroupIds.add(entry.getTabGroupId());
            // TODO(b/336589861): Set a real saved tab group ID from its corresponding sync entity
            // here.
            savedTabGroupIds.add("");
            groupTitles.add(entry.getGroupTitle() == null ? "" : entry.getGroupTitle());
            groupColors.add(entry.getGroupColor());
            for (Tab tab : entry.getTabs()) {
                WebContentsState tabWebContentsState = getWebContentsState(tab);
                allTabs.add(tab);
                perTabRootId.add(entry.getRootId());
                byteBuffers.add(tabWebContentsState.buffer());
                savedStateVersions.add(tabWebContentsState.version());
            }
        }

        // If there is only a single valid tab remaining save it individually.
        if (validEntries.size() == 1 && validEntries.get(0).isSingleTab()) {
            createHistoricalTabInternal(allTabs.get(0));
            return;
        }

        // If there is only a single entry and more than one tab remaining so this is a group.
        if (validEntries.size() == 1 && !validEntries.get(0).isSingleTab()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Tabs.RecentlyClosed.HistoricalSaverCloseType",
                    HistoricalSaverCloseType.GROUP,
                    HistoricalSaverCloseType.COUNT);
            HistoricalTabSaverImplJni.get()
                    .createHistoricalGroup(
                            mTabModel,
                            tabGroupIds.get(0),
                            savedTabGroupIds.get(0),
                            groupTitles.get(0),
                            groupColors.get(0),
                            allTabs.toArray(new Tab[0]),
                            byteBuffers.toArray(new ByteBuffer[0]),
                            CollectionUtil.integerCollectionToIntArray(savedStateVersions));
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Tabs.RecentlyClosed.HistoricalSaverCloseType",
                HistoricalSaverCloseType.BULK,
                HistoricalSaverCloseType.COUNT);
        HistoricalTabSaverImplJni.get()
                .createHistoricalBulkClosure(
                        mTabModel,
                        CollectionUtil.integerCollectionToIntArray(rootIds),
                        tabGroupIds.toArray(new Token[0]),
                        savedTabGroupIds.toArray(new String[0]),
                        groupTitles.toArray(new String[0]),
                        CollectionUtil.integerCollectionToIntArray(groupColors),
                        CollectionUtil.integerCollectionToIntArray(perTabRootId),
                        allTabs.toArray(new Tab[0]),
                        byteBuffers.toArray(new ByteBuffer[0]),
                        CollectionUtil.integerCollectionToIntArray(savedStateVersions));
    }

    private void createHistoricalTabInternal(Tab tab) {
        RecordHistogram.recordEnumeratedHistogram(
                "Tabs.RecentlyClosed.HistoricalSaverCloseType",
                HistoricalSaverCloseType.TAB,
                HistoricalSaverCloseType.COUNT);
        HistoricalTabSaverImplJni.get()
                .createHistoricalTab(
                        tab, getWebContentsState(tab).buffer(), getWebContentsState(tab).version());
    }

    /**
     * Checks that the tab has a valid URL for saving. This requires the URL to exist and not be an
     * internal Chrome scheme, about:blank, or a native page and it cannot be incognito.
     */
    private boolean shouldSave(Tab tab) {
        if (tab.isIncognito()) return false;
        // Check the secondary tab model to see if the tab was moved instead of deleted.
        if (tabIdExistsInSecondaryModel(tab.getId())) return false;

        // {@link GURL#getScheme()} is not available in unit tests.
        if (mIgnoreUrlSchemesForTesting) return true;

        GURL committedUrlOrFrozenUrl;
        if (tab.getWebContents() != null) {
            committedUrlOrFrozenUrl = tab.getWebContents().getLastCommittedUrl();
        } else {
            if (tab.getWebContentsState() == null) return false;

            committedUrlOrFrozenUrl = tab.getUrl();
        }

        return committedUrlOrFrozenUrl != null
                && committedUrlOrFrozenUrl.isValid()
                && !committedUrlOrFrozenUrl.isEmpty()
                && !UNSUPPORTED_SCHEMES.contains(committedUrlOrFrozenUrl.getScheme());
    }

    private boolean tabIdExistsInSecondaryModel(int tabId) {
        for (Supplier<TabModel> tabModelSupplier : mSecondaryTabModelSuppliers) {
            if (tabModelSupplier.hasValue() && tabModelSupplier.get().getTabById(tabId) != null) {
                return true;
            }
        }

        return false;
    }

    /**
     * Generate a valid list of {@link HistoricalEntry}s. Filter out {@link Tab}s that do not pass
     * the {@link #shouldSave(Tab)}.
     */
    private List<Tab> getValidatedTabs(List<Tab> tabs) {
        List<Tab> validatedTabs = new ArrayList<>();
        for (Tab tab : tabs) {
            if (!shouldSave(tab)) continue;

            validatedTabs.add(tab);
        }
        return validatedTabs;
    }

    /**
     * Generate a valid list of {@link HistoricalEntry}s.
     * - Filter out {@link Tab}s that do not pass the {@link #shouldSave(Tab)}.
     * - Drop {@link HistoricalEntry} if empty after validation.
     */
    private List<HistoricalEntry> getValidatedEntries(List<HistoricalEntry> entries) {
        List<HistoricalEntry> validatedEntries = new ArrayList<>();
        for (HistoricalEntry entry : entries) {
            List<Tab> validTabs = getValidatedTabs(entry.getTabs());
            if (validTabs.isEmpty()) continue;

            boolean saveAsSingleTab = validTabs.size() == 1 && entry.getTabGroupId() == null;
            if (saveAsSingleTab) {
                validatedEntries.add(new HistoricalEntry(validTabs.get(0)));
                continue;
            }
            validatedEntries.add(
                    new HistoricalEntry(
                            entry.getRootId(),
                            entry.getTabGroupId(),
                            entry.getGroupTitle(),
                            entry.getGroupColor(),
                            validTabs));
        }
        return validatedEntries;
    }

    private static WebContentsState getWebContentsState(Tab tab) {
        WebContentsState tempState = WebContentsState.getTempWebContentsState();
        // If WebContents exists, on the native side during frozen tab restoration the same check
        // will be made and return the contents immediately, skipping the logic that requires
        // restoring from the WebContentsState. This tempState acts as an empty object placeholder.
        if (tab.getWebContents() != null) return tempState;

        WebContentsState state = tab.getWebContentsState();
        return (state == null) ? tempState : state;
    }

    void ignoreUrlSchemesForTesting(boolean ignore) {
        mIgnoreUrlSchemesForTesting = ignore;
    }

    @NativeMethods
    interface Natives {
        void createHistoricalTab(Tab tab, ByteBuffer state, int savedStateVersion);

        void createHistoricalGroup(
                TabModel model,
                Token token,
                @JniType("std::u16string") String savedTabGroupId,
                @JniType("std::u16string") String title,
                int color,
                Tab[] tabs,
                ByteBuffer[] byteBuffers,
                @JniType("std::vector<int32_t>") int[] savedStationsVersions);

        void createHistoricalBulkClosure(
                TabModel model,
                @JniType("std::vector<int32_t>") int[] rootIds,
                Token[] tabGroupIds,
                @JniType("std::vector<std::u16string>") String[] savedTabGroupIds,
                @JniType("std::vector<std::u16string>") String[] titles,
                @JniType("std::vector<int32_t>") int[] colors,
                @JniType("std::vector<int32_t>") int[] perTabRootId,
                Tab[] tabs,
                ByteBuffer[] byteBuffers,
                @JniType("std::vector<int32_t>") int[] savedStateVersions);
    }
}
