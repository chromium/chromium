// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.pseudotab;

import android.content.Context;
import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.annotation.concurrent.GuardedBy;

/**
 * Representation of a Tab-like card in the Grid Tab Switcher.
 */
public class PseudoTab {
    private static final String TAG = "PseudoTab";

    private final Integer mTabId;
    private final WeakReference<Tab> mTab;

    @GuardedBy("sLock")
    private static final Map<Integer, PseudoTab> sAllTabs = new LinkedHashMap<>();
    private static final Object sLock = new Object();
    private static boolean sReadStateFile;
    private static List<PseudoTab> sAllTabsFromStateFile;
    private static PseudoTab sActiveTabFromStateFile;

    /**
     * An interface to get the title to be used for a tab.
     */
    public interface TitleProvider {
        String getTitle(Context context, PseudoTab tab);
    }

    /**
     * Construct from a tab ID. An earlier instance with the same ID can be returned.
     */
    public static PseudoTab fromTabId(int tabId) {
        synchronized (sLock) {
            PseudoTab cached = sAllTabs.get(tabId);
            if (cached != null) return cached;
            return new PseudoTab(tabId);
        }
    }

    private PseudoTab(int tabId) {
        mTabId = tabId;
        mTab = null;
        sAllTabs.put(getId(), this);
    }

    /**
     * Construct from a {@link Tab}. An earlier instance with the same {@link Tab} can be returned.
     */
    public static PseudoTab fromTab(@NonNull Tab tab) {
        synchronized (sLock) {
            PseudoTab cached = sAllTabs.get(tab.getId());
            if (cached != null && cached.hasRealTab()) {
                if (cached.getTab() == tab) {
                    return cached;
                } else {
                    assert tab.getWebContents() == null || cached.getTab().getWebContents() == null
                            || cached.getTab().getWebContents().getTopLevelNativeWindow() == null;
                    return new PseudoTab(tab);
                }
            }
            // We need to upgrade a pre-native Tab to a post-native Tab.
            return new PseudoTab(tab);
        }
    }

    private PseudoTab(@NonNull Tab tab) {
        mTabId = tab.getId();
        mTab = new WeakReference<>(tab);
        sAllTabs.put(getId(), this);
    }

    /**
     * Convert a list of {@link Tab} to a list of {@link PseudoTab}.
     * @param tabs A list of {@link Tab}
     * @return A list of {@link PseudoTab}
     */
    public static List<PseudoTab> getListOfPseudoTab(@Nullable List<Tab> tabs) {
        List<PseudoTab> pseudoTabs = null;
        if (tabs != null) {
            pseudoTabs = new ArrayList<>();
            for (Tab tab : tabs) {
                pseudoTabs.add(fromTab(tab));
            }
        }
        return pseudoTabs;
    }

    /**
     * Convert a {@link TabList} to a list of {@link PseudoTab}.
     * @param tabList A {@link TabList}
     * @return A list of {@link PseudoTab}
     */
    public static List<PseudoTab> getListOfPseudoTab(@Nullable TabList tabList) {
        List<PseudoTab> pseudoTabs = null;
        if (tabList != null) {
            pseudoTabs = new ArrayList<>();
            for (int i = 0; i < tabList.getCount(); i++) {
                Tab tab = tabList.getTabAt(i);
                if (tab != null) {
                    pseudoTabs.add(fromTab(tab));
                }
            }
        }
        return pseudoTabs;
    }

    @Override
    public String toString() {
        assert mTabId != null;
        return "Tab " + mTabId;
    }

    /**
     * @return The ID of the {@link PseudoTab}
     */
    public int getId() {
        return mTabId;
    }

    /**
     * Get the title of the {@link PseudoTab} through a {@link TitleProvider}.
     *
     * If the {@link TitleProvider} is {@code null}, fall back to {@link #getTitle()}.
     *
     * @param context The activity context.
     * @param titleProvider The {@link TitleProvider} to provide the title.
     * @return The title
     */
    public String getTitle(Context context, @Nullable TitleProvider titleProvider) {
        if (titleProvider != null) return titleProvider.getTitle(context, this);
        return getTitle();
    }

    /**
     * Get the title of the {@link PseudoTab}.
     * @return The title
     */
    public String getTitle() {
        if (mTab != null && mTab.get() != null && mTab.get().isInitialized()) {
            return mTab.get().getTitle();
        }
        assert mTabId != null;
        return TabAttributeCache.getTitle(mTabId);
    }

    /**
     * Get the URL of the {@link PseudoTab}.
     * @return The URL
     */
    public GURL getUrl() {
        if (mTab != null && mTab.get() != null && mTab.get().isInitialized()) {
            return mTab.get().getUrl();
        }
        assert mTabId != null;
        return TabAttributeCache.getUrl(mTabId);
    }

    /**
     * Get the root ID of the {@link PseudoTab}.
     * @return The root ID
     */
    public int getRootId() {
        if (mTab != null && mTab.get() != null && mTab.get().isInitialized()) {
            return CriticalPersistedTabData.from(mTab.get()).getRootId();
        }
        assert mTabId != null;
        return TabAttributeCache.getRootId(mTabId);
    }

    /**
     * @return The timestamp of the {@link PseudoTab}.
     */
    public long getTimestampMillis() {
        if (mTab != null && mTab.get() != null && mTab.get().isInitialized()) {
            return CriticalPersistedTabData.from(mTab.get()).getTimestampMillis();
        }
        assert mTabId != null;
        return TabAttributeCache.getTimestampMillis(mTabId);
    }

    /**
     * @return Whether the {@link PseudoTab} is in the Incognito mode.
     */
    public boolean isIncognito() {
        if (mTab != null && mTab.get() != null) return mTab.get().isIncognito();
        assert mTabId != null;
        return false;
    }

    /**
     * @return Whether an underlying real {@link Tab} is available.
     */
    public boolean hasRealTab() {
        return getTab() != null;
    }

    /**
     * Get the underlying real {@link Tab}. We should avoid using this.
     * @return The underlying real {@link Tab}.
     */
    @Deprecated
    public @Nullable Tab getTab() {
        if (mTab != null) return mTab.get();
        return null;
    }

    /**
     * Clear the internal static storage as if the app is restarted.
     * This should/can be called when emulating restarting in instrumented tests, or between
     * Robolectric tests.
     */
    @VisibleForTesting
    public static void clearForTesting() {
        synchronized (sLock) {
            sAllTabs.clear();
            sReadStateFile = false;
            sActiveTabFromStateFile = null;
            if (sAllTabsFromStateFile != null) {
                sAllTabsFromStateFile.clear();
            }
        }
    }

    /**
     * Get related tabs of a certain {@link PseudoTab}, through {@link TabModelFilter}s if
     * available.
     *
     * @param context The activity context.
     * @param member The {@link PseudoTab} related to
     * @param tabModelSelector The {@link TabModelSelector} to query the tab relation
     * @return Related {@link PseudoTab}s
     */
    public static @NonNull List<PseudoTab> getRelatedTabs(
            Context context, PseudoTab member, @NonNull TabModelSelector tabModelSelector) {
        synchronized (sLock) {
            List<Tab> relatedTabs = getRelatedTabList(tabModelSelector, member.getId());
            if (relatedTabs != null) return getListOfPseudoTab(relatedTabs);

            List<PseudoTab> related = new ArrayList<>();
            int rootId = member.getRootId();
            if (rootId == Tab.INVALID_TAB_ID
                    || !TabUiFeatureUtilities.isTabGroupsAndroidEnabled(context)) {
                related.add(member);
                return related;
            }
            for (Integer key : sAllTabs.keySet()) {
                PseudoTab tab = sAllTabs.get(key);
                assert tab != null;
                if (tab.getRootId() == Tab.INVALID_TAB_ID) continue;
                if (tab.getRootId() != rootId) continue;
                related.add(tab);
            }
            assert related.size() > 0;
            return related;
        }
    }

    private static @Nullable List<Tab> getRelatedTabList(
            @NonNull TabModelSelector tabModelSelector, int tabId) {
        if (!tabModelSelector.isTabStateInitialized()) {
            assert ChromeFeatureList.sInstantStart.isEnabled();
            return null;
        }
        TabModelFilterProvider provider = tabModelSelector.getTabModelFilterProvider();
        List<Tab> related = provider.getTabModelFilter(false).getRelatedTabList(tabId);
        if (related.size() > 0) return related;
        related = provider.getTabModelFilter(true).getRelatedTabList(tabId);
        assert related.size() > 0;
        return related;
    }

    @VisibleForTesting
    static int getAllTabsCountForTests() {
        synchronized (sLock) {
            return sAllTabs.size();
        }
    }

    /**
     * @param context The activity context.
     */
    @Nullable
    public static List<PseudoTab> getAllPseudoTabsFromStateFile(Context context) {
        readAllPseudoTabsFromStateFile(context);
        return sAllTabsFromStateFile;
    }

    @Nullable
    public static PseudoTab getActiveTabFromStateFile(Context context) {
        readAllPseudoTabsFromStateFile(context);
        return sActiveTabFromStateFile;
    }

    @SuppressWarnings("AssertionSideEffect")
    private static void readAllPseudoTabsFromStateFile(Context context) {
        assert ChromeFeatureList.sInstantStart.isEnabled();
        if (sReadStateFile) return;
        sReadStateFile = true;

        long startMs = SystemClock.elapsedRealtime();
        File stateFile = new File(TabStateDirectory.getOrCreateTabbedModeStateDirectory(),
                TabbedModeTabPersistencePolicy.getStateFileName(0));
        if (!stateFile.exists()) {
            Log.i(TAG, "State file does not exist.");
            return;
        }
        FileInputStream stream = null;
        byte[] data;
        try {
            stream = new FileInputStream(stateFile);
            data = new byte[(int) stateFile.length()];
            stream.read(data);
        } catch (IOException exception) {
            Log.e(TAG, "Could not read state file.", exception);
            return;
        } finally {
            StreamUtil.closeQuietly(stream);
        }
        Log.i(TAG, "Finished fetching tab list.");
        DataInputStream dataStream = new DataInputStream(new ByteArrayInputStream(data));

        Set<Integer> seenRootId = new HashSet<>();
        sAllTabsFromStateFile = new ArrayList<>();
        try {
            TabPersistentStore.readSavedStateFile(dataStream,
                    (index, id, url, isIncognito, isStandardActiveIndex, isIncognitoActiveIndex)
                            -> {
                        // Skip restoring of non-selected NTP to match the real restoration logic.
                        if (UrlUtilities.isCanonicalizedNTPUrl(url) && !isStandardActiveIndex) {
                            return;
                        }
                        PseudoTab tab = PseudoTab.fromTabId(id);
                        if (isStandardActiveIndex) {
                            assert sActiveTabFromStateFile == null;
                            sActiveTabFromStateFile = tab;
                        }
                        int rootId = tab.getRootId();
                        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(context)
                                && seenRootId.contains(rootId)) {
                            return;
                        }
                        sAllTabsFromStateFile.add(tab);
                        seenRootId.add(rootId);
                    },
                    null);
        } catch (IOException exception) {
            Log.e(TAG, "Could not read state file.", exception);
            return;
        }

        Log.d(TAG, "All pre-native tabs: " + sAllTabsFromStateFile);
        Log.i(TAG, "readAllPseudoTabsFromStateFile() took %dms",
                SystemClock.elapsedRealtime() - startMs);
    }
}
