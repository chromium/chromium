// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CollectionUtil;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
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

/**
 * Creates historical entries in TabRestoreService.
 */
@JNINamespace("historical_tab_saver")
public class HistoricalTabSaverImpl implements HistoricalTabSaver {
    private static final List<String> UNSUPPORTED_SCHEMES =
            new ArrayList<>(Arrays.asList(UrlConstants.CHROME_SCHEME,
                    UrlConstants.CHROME_NATIVE_SCHEME, ContentUrlConstants.ABOUT_SCHEME));
    private final TabModel mTabModel;
    private boolean mIgnoreUrlSchemesForTesting;

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({HistoricalSaverCloseType.TAB, HistoricalSaverCloseType.GROUP,
            HistoricalSaverCloseType.BULK, HistoricalSaverCloseType.COUNT})
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
        List<Integer> perTabGroupId = new ArrayList<>();

        // Distinct group IDs that will be saved - one per group.
        List<Integer> groupIds = new ArrayList<>();
        // Titles corresponding to each element in groupIds.
        List<String> groupTitles = new ArrayList<>();

        // Byte buffer associated with WebContentsState per tab by index.
        List<ByteBuffer> byteBuffers = new ArrayList<>();
        // Saved state version of WebContentsState per tab by index.
        List<Integer> savedStateVersions = new ArrayList<>();

        for (HistoricalEntry entry : validEntries) {
            if (entry.isSingleTab()) {
                WebContentsState tabWebContentsState = getWebContentsState(entry.getTabs().get(0));
                allTabs.add(entry.getTabs().get(0));
                perTabGroupId.add(Tab.INVALID_TAB_ID);
                byteBuffers.add(tabWebContentsState.buffer());
                savedStateVersions.add(tabWebContentsState.version());
                continue;
            }

            groupIds.add(entry.getGroupId());
            groupTitles.add(entry.getGroupTitle() == null ? "" : entry.getGroupTitle());
            for (Tab tab : entry.getTabs()) {
                WebContentsState tabWebContentsState = getWebContentsState(tab);
                allTabs.add(tab);
                perTabGroupId.add(entry.getGroupId());
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
                    "Tabs.RecentlyClosed.HistoricalSaverCloseType", HistoricalSaverCloseType.GROUP,
                    HistoricalSaverCloseType.COUNT);
            HistoricalTabSaverImplJni.get().createHistoricalGroup(mTabModel, groupTitles.get(0),
                    allTabs.toArray(new Tab[0]), byteBuffers.toArray(new ByteBuffer[0]),
                    CollectionUtil.integerListToIntArray(savedStateVersions));
            return;
        }

        // IDs are passed only to group tabs. New IDs are generated when saving.
        RecordHistogram.recordEnumeratedHistogram("Tabs.RecentlyClosed.HistoricalSaverCloseType",
                HistoricalSaverCloseType.BULK, HistoricalSaverCloseType.COUNT);
        HistoricalTabSaverImplJni.get().createHistoricalBulkClosure(mTabModel,
                CollectionUtil.integerListToIntArray(groupIds), groupTitles.toArray(new String[0]),
                CollectionUtil.integerListToIntArray(perTabGroupId), allTabs.toArray(new Tab[0]),
                byteBuffers.toArray(new ByteBuffer[0]),
                CollectionUtil.integerListToIntArray(savedStateVersions));
    }

    private void createHistoricalTabInternal(Tab tab) {
        RecordHistogram.recordEnumeratedHistogram("Tabs.RecentlyClosed.HistoricalSaverCloseType",
                HistoricalSaverCloseType.TAB, HistoricalSaverCloseType.COUNT);
        HistoricalTabSaverImplJni.get().createHistoricalTab(
                tab, getWebContentsState(tab).buffer(), getWebContentsState(tab).version());
    }

    /**
     * Checks that the tab has a valid URL for saving. This requires the URL to exist and not be an
     * internal Chrome scheme, about:blank, or a native page and it cannot be incognito.
     */
    private boolean shouldSave(Tab tab) {
        if (tab.isIncognito()) return false;

        // {@link GURL#getScheme()} is not available in unit tests.
        if (mIgnoreUrlSchemesForTesting) return true;

        GURL committedUrlOrFrozenUrl;
        if (tab.getWebContents() != null) {
            committedUrlOrFrozenUrl = tab.getWebContents().getLastCommittedUrl();
        } else {
            if (CriticalPersistedTabData.from(tab).getWebContentsState() == null) return false;

            committedUrlOrFrozenUrl = tab.getUrl();
        }

        return committedUrlOrFrozenUrl != null && committedUrlOrFrozenUrl.isValid()
                && !committedUrlOrFrozenUrl.isEmpty()
                && !UNSUPPORTED_SCHEMES.contains(committedUrlOrFrozenUrl.getScheme());
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
     * - Remove title and id from a single tab entry.
     * - Drop {@link HistoricalEntry} if empty after validation.
     */
    private List<HistoricalEntry> getValidatedEntries(List<HistoricalEntry> entries) {
        List<HistoricalEntry> validatedEntries = new ArrayList<>();
        for (HistoricalEntry entry : entries) {
            List<Tab> validTabs = getValidatedTabs(entry.getTabs());
            if (validTabs.isEmpty()) continue;

            if (validTabs.size() == 1) {
                validatedEntries.add(new HistoricalEntry(validTabs.get(0)));
                continue;
            }
            validatedEntries.add(
                    new HistoricalEntry(entry.getGroupId(), entry.getGroupTitle(), validTabs));
        }
        return validatedEntries;
    }

    private static WebContentsState getWebContentsState(Tab tab) {
        WebContentsState tempState = WebContentsState.getTempWebContentsState();
        // If WebContents exists, on the native side during frozen tab restoration the same check
        // will be made and return the contents immediately, skipping the logic that requires
        // restoring from the WebContentsState. This tempState acts as an empty object placeholder.
        if (tab.getWebContents() != null) return tempState;

        WebContentsState state = CriticalPersistedTabData.from(tab).getWebContentsState();
        return (state == null) ? tempState : state;
    }

    @VisibleForTesting
    void ignoreUrlSchemesForTesting(boolean ignore) {
        mIgnoreUrlSchemesForTesting = ignore;
    }

    @NativeMethods
    interface Natives {
        void createHistoricalTab(Tab tab, ByteBuffer state, int savedStateVersion);
        void createHistoricalGroup(TabModel model, String title, Tab[] tabs,
                ByteBuffer[] byteBuffers, int[] savedStationsVersions);
        void createHistoricalBulkClosure(TabModel model, int[] groupIds, String[] titles,
                int[] perTabGroupId, Tab[] tabs, ByteBuffer[] byteBuffers,
                int[] savedStateVersions);
    }
}
