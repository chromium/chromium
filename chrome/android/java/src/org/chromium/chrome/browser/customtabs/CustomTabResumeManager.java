// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.view.View;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Coordinates Custom Tab state restoration and auto-saves. Handles TabState storage, loading, and
 * auto-saves for Custom Tab resumption.
 */
@NullMarked
public class CustomTabResumeManager {
    private static final String TAG = "CustomTabResumeMgr";
    private static final String CCT_TAB_DATA_DIR = "cct_tab_data";

    private static final int IN_MEMORY_CACHE_MAX_SIZE = 4;
    private static final int DEFAULT_MAX_RESUMPTION_TABS = 20;
    private static final long DEFAULT_RESUMPTION_TAB_TTL_MS = TimeUnit.DAYS.toMillis(5);

    private static final Map<String, TabState> sInMemoryStateCache = new LinkedHashMap<>(IN_MEMORY_CACHE_MAX_SIZE + 1, 0.75f, true) {
                @Override
                protected boolean removeEldestEntry(Map.Entry<String, TabState> eldest) {
                    return size() > IN_MEMORY_CACHE_MAX_SIZE;
                }
            };

    /** Extra indicating if the tab should be restored from a previously saved state. */
    static final String EXTRA_RESTORE_TAB =
            "org.chromium.chrome.browser.customtabs.EXTRA_RESTORE_TAB";

    /** Extra supplying the embedder's unique session/tab identifier for state mapping. */
    static final String EXTRA_EMBEDDER_TAB_ID =
            "org.chromium.chrome.browser.customtabs.EXTRA_TAB_DATA_ID";

    /** Extra defining the maximum number of tabs to persist on disk for resumption. */
    static final String EXTRA_MAX_RESUMPTION_TABS =
            "org.chromium.chrome.browser.customtabs.EXTRA_MAX_RESUMPTION_TABS";

    /** Extra defining the time-to-live (TTL) duration in milliseconds for persisted tabs. */
    static final String EXTRA_RESUMPTION_TAB_TTL_MS =
            "org.chromium.chrome.browser.customtabs.EXTRA_RESUMPTION_TAB_TTL_MS";

    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CipherFactory mCipherFactory;
    private final Map<Integer, String> mTabIdToEmbedderTabIdMap = new HashMap<>();
    private final Set<Tab> mObservedTabs = new HashSet<>();
    private final TabObserver mTabObserver;

    /**
     * Determines whether a resumption manager should be created based on intent extras.
     *
     * @param intentData The intent provider to inspect.
     * @return True if a resumption manager should be created, false otherwise.
     */
    public static boolean shouldCreateTabResumeManager(
            @Nullable BrowserServicesIntentDataProvider intentData) {
        if (intentData == null) return false;

        return getEmbedderTabId(intentData.getIntent()) != null;
    }

    /**
     * Checks if the intent explicitly requests tab resumption.
     *
     * @param intent The incoming launch intent.
     * @return True if tab resumption is requested, false otherwise.
     */
    public static boolean isTabResumptionRequested(@Nullable Intent intent) {
        if (intent == null) return false;

        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_RESTORE_TAB, false)
                && getEmbedderTabId(intent) != null;
    }

    /**
     * Evaluates if a new intent requires a relaunch of the Custom Tab activity.
     *
     * @param intentData The new intent provider.
     * @param currentIntentData The active intent provider.
     * @return True if a relaunch is forced for resumption, false otherwise.
     */
    public static boolean shouldForceRelaunchForResumption(
            @Nullable BrowserServicesIntentDataProvider intentData,
            @Nullable BrowserServicesIntentDataProvider currentIntentData) {
        if (intentData == null) return false;

        Intent intent = intentData.getIntent();
        // Relaunch if the new intent explicitly requests a tab restoration.
        if (isTabResumptionRequested(intent)) {
            return true;
        }

        if (currentIntentData == null) return false;

        // Relaunch if switching between different resumption sessions (or switching
        // between a resumption session and a normal session) to prevent history contamination.
        String newEmbedderTabId = getEmbedderTabId(intent);
        String currentEmbedderTabId = getEmbedderTabId(currentIntentData.getIntent());
        return !Objects.equals(newEmbedderTabId, currentEmbedderTabId);
    }

    /**
     * Constructs a resumption manager for the given intent provider.
     *
     * @param intentDataProvider The intent provider of the CCT session.
     * @param cipherFactory The cipher factory for encrypting persisted state.
     */
    public CustomTabResumeManager(
            BrowserServicesIntentDataProvider intentDataProvider, CipherFactory cipherFactory) {
        mIntentDataProvider = intentDataProvider;
        mCipherFactory = cipherFactory;
        mTabObserver =
                new TabObserver() {
                    @Override
                    public void onHidden(Tab tab, int reason) {
                        Log.i(
                                TAG,
                                "mTabObserver: onHidden triggered for tab %d, reason: %d",
                                tab.getId(),
                                reason);
                        saveTabStateDeferred(tab);
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        Log.i(
                                TAG,
                                "mTabObserver: onDestroyed triggered for tab %d, isHidden = %b",
                                tab.getId(),
                                tab.isHidden());
                        if (!tab.isHidden()) {
                            saveTabStateDeferred(tab);
                        }
                        tab.removeObserver(this);
                        mTabIdToEmbedderTabIdMap.remove(tab.getId());
                        mObservedTabs.remove(tab);
                    }
                };
    }

    /**
     * Checks if tab resumption is requested for the CCT session.
     *
     * @return True if resumption is requested, false otherwise.
     */
    public boolean isTabResumptionRequested() {
        return isTabResumptionRequested(mIntentDataProvider.getIntent());
    }

    /**
     * Attempts to restore a previously saved tab state for the CCT session.
     *
     * @param cipherFactory The cipher factory for decrypting state files.
     * @param profile The profile associated with the CCT.
     * @param tabModelSelector The selector to retrieve the active tab model.
     * @param isOffTheRecord Whether the tab is in incognito mode.
     * @param windowAndroid The window helper for the activity.
     * @param delegateFactory Factory to create tab delegates.
     * @param appId The ID of the application that launched the CCT.
     * @return The restored {@link Tab}, or {@code null} if restoration is not possible.
     */
    public @Nullable Tab maybeRestoreTab(
            CipherFactory cipherFactory,
            Profile profile,
            TabModelSelector tabModelSelector,
            boolean isOffTheRecord,
            WindowAndroid windowAndroid,
            TabDelegateFactory delegateFactory,
            String appId) {
        if (!isTabResumptionRequested()) return null;

        String embedderTabId = getEmbedderTabId();
        if (embedderTabId == null) return null;

        TabState restoredState = restoreTabState(embedderTabId, cipherFactory);
        if (restoredState == null) return null;

        int tabId = getTabId(embedderTabId);
        TabModel tabModel = tabModelSelector.getModel(isOffTheRecord);
        closeExistingTabsForEmbedderTabId(embedderTabId, tabModel, tabId);

        return createTabFromState(
                profile, windowAndroid, delegateFactory, restoredState, tabId, appId);
    }

    /**
     * Registers the tab with the resumption manager to enable auto-saving of its state during the
     * session.
     *
     * @param tab The active tab to monitor.
     */
    public void registerTabIfResumptionEnabled(Tab tab) {
        String embedderTabId = getEmbedderTabId();
        if (embedderTabId != null) {
            registerTab(tab, embedderTabId);
        }
    }

    /**
     * Returns the {@link TabCreationMode} for the given tab, falling back to the default if the tab
     * was not restored.
     *
     * @param tab The tab being initialized.
     * @param fallbackMode The fallback creation mode.
     * @return The {@link TabCreationMode} indicating how the tab was created.
     */
    public @TabCreationMode int getTabCreationMode(Tab tab, @TabCreationMode int fallbackMode) {
        if (tab.getLaunchType() == TabLaunchType.FROM_RESTORE) {
            return TabCreationMode.RESTORED;
        }
        return fallbackMode;
    }

    /**
     * Determines whether the given tab should be added to the active tab model.
     *
     * @param mode The tab creation mode.
     * @param tab The tab to evaluate.
     * @param restoredTab A tab that was restored via standard activity restoration, if any.
     * @param tabModel The active tab model.
     * @return {@code true} if the tab needs to be added, {@code false} otherwise.
     */
    public boolean shouldAddTabToModel(
            @TabCreationMode int mode, Tab tab, @Nullable Tab restoredTab, TabModel tabModel) {
        return restoredTab == null && tabModel.indexOf(tab) == TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Safely requests focus for the tab view, handling cases where the tab is currently frozen.
     *
     * @param tab The tab to request focus on.
     */
    public void requestFocus(Tab tab) {
        View tabView = tab.getView();
        if (tabView != null) {
            requestFocusInternal(tabView);
        } else {
            Log.i(
                    TAG,
                    "requestFocus: tabView is null (frozen tab), deferring requestFocus until"
                            + " content changes");
            tab.addObserver(
                    new TabObserver() {
                        @Override
                        public void onContentChanged(Tab tab) {
                            View view = tab.getView();
                            Log.i(
                                    TAG,
                                    "requestFocus deferred: onContentChanged called. view = %s",
                                    view);
                            if (view != null) {
                                requestFocusInternal(view);
                                tab.removeObserver(this);
                            }
                        }
                    });
        }
    }

    /** Cleans up observers and releases references on destruction of the manager. */
    public void destroy() {
        for (Tab tab : mObservedTabs) {
            tab.removeObserver(mTabObserver);
        }
        mObservedTabs.clear();
        mTabIdToEmbedderTabIdMap.clear();
    }

    // ==========================================
    // Private instance helper methods
    // ==========================================

    private @Nullable String getEmbedderTabId() {
        return getEmbedderTabId(mIntentDataProvider.getIntent());
    }

    private void registerTab(Tab tab, String embedderTabId) {
        Log.i(
                TAG,
                "registerTab: registering tab %d under embedderTabId: %s",
                tab.getId(),
                embedderTabId);
        mTabIdToEmbedderTabIdMap.put(tab.getId(), embedderTabId);
        mObservedTabs.add(tab);
        tab.addObserver(mTabObserver);
    }

    private void closeExistingTabsForEmbedderTabId(
            String embedderTabId, TabModel tabModel, int tabId) {
        int registeredTabId = getActiveTabIdForEmbedderTabId(embedderTabId);
        List<Tab> tabsToClose = new ArrayList<>();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab t = tabModel.getTabAt(i);
            if (t != null && (t.getId() == tabId || t.getId() == registeredTabId)) {
                tabsToClose.add(t);
            }
        }
        for (Tab t : tabsToClose) {
            Log.i(
                    TAG,
                    "closeExistingTabsForEmbedderTabId: Found existing tab with tabId %d"
                            + " in TabModel, closing it.",
                    t.getId());
            tabModel.getTabRemover()
                    .closeTabs(
                            TabClosureParams.closeTab(t).allowUndo(false).build(),
                            /* allowDialog= */ false);
        }
    }

    private int getActiveTabIdForEmbedderTabId(String embedderTabId) {
        for (Map.Entry<Integer, String> entry : mTabIdToEmbedderTabIdMap.entrySet()) {
            if (entry.getValue().equals(embedderTabId)) {
                return entry.getKey();
            }
        }
        return Tab.INVALID_TAB_ID;
    }

    private void saveTabStateDeferred(Tab tab) {
        String embedderTabId = mTabIdToEmbedderTabIdMap.get(tab.getId());
        Log.i(
                TAG,
                "saveTabStateDeferred: tab: %d, resolved embedderTabId: %s",
                tab.getId(),
                embedderTabId);
        if (embedderTabId == null) return;

        TabState tabState = TabStateExtractor.from(tab);
        if (tabState == null) {
            Log.i(TAG, "saveTabStateDeferred: TabStateExtractor.from() returned null");
            return;
        }

        TabState clonedState = cloneTabState(tabState);
        if (clonedState == null) {
            Log.i(TAG, "saveTabStateDeferred: cloneTabState returned null");
            return;
        }
        sInMemoryStateCache.put(embedderTabId, clonedState);

        int tabId = getTabId(embedderTabId);
        int maxTabs = getMaxTabsLimit(mIntentDataProvider.getIntent());
        long ttlMs = getTabTtlLimit(mIntentDataProvider.getIntent());
        CipherFactory cipherFactory = mCipherFactory;

        Log.i(
                TAG,
                "saveTabStateDeferred: posting saveState task to background thread for tabId: %d",
                tabId);
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> saveStateAndPruneDisk(clonedState, tabId, maxTabs, ttlMs, cipherFactory));
    }

    // ==========================================
    // Private static helper methods
    // ==========================================

    private static @Nullable String getEmbedderTabId(@Nullable Intent intent) {
        if (intent == null) return null;

        return IntentUtils.safeGetStringExtra(intent, EXTRA_EMBEDDER_TAB_ID);
    }

    private static @Nullable TabState restoreTabState(
            String embedderTabId, CipherFactory cipherFactory) {
        if (sInMemoryStateCache.containsKey(embedderTabId)) {
            Log.i(
                    TAG,
                    "restoreTabState: Found state in memory cache for embedderTabId = %s",
                    embedderTabId);
            return cloneTabState(sInMemoryStateCache.get(embedderTabId));
        }

        int tabId = getTabId(embedderTabId);
        Log.i(TAG, "restoreTabState: embedderTabId = %s, tabId = %d", embedderTabId, tabId);
        try {
            TabState state =
                    TabStateFileManager.restoreTabState(
                            getStorageDirectory(), tabId, cipherFactory);
            Log.i(
                    TAG,
                    "restoreTabState: restoreTabState returned %s TabState",
                    state != null ? "NON-NULL" : "NULL");
            if (state != null) {
                sInMemoryStateCache.put(embedderTabId, cloneTabState(state));
            }
            return cloneTabState(state);
        } catch (Exception e) {
            Log.e(TAG, "Failed to restore TabState for CCT resumption", e);
            return null;
        }
    }

    private static Tab createTabFromState(
            Profile profile,
            WindowAndroid windowAndroid,
            TabDelegateFactory delegateFactory,
            TabState state,
            int tabId,
            String appId) {
        Tab tab =
                TabBuilder.createFromFrozenState(profile)
                        .setId(tabId)
                        .setWindow(windowAndroid)
                        .setDelegateFactory(delegateFactory)
                        .setInitiallyHidden(false)
                        .setTabState(state)
                        .setContentViewDeferred(false)
                        .build();
        TabAssociatedApp.from(tab).setAppId(appId);
        return tab;
    }

    private static File getStorageDirectory() {
        return new File(ContextUtils.getApplicationContext().getFilesDir(), CCT_TAB_DATA_DIR);
    }

    private static int getTabId(String embedderTabId) {
        return Math.max(1, embedderTabId.hashCode() & 0x7fffffff);
    }

    private static int getMaxTabsLimit(@Nullable Intent intent) {
        if (intent == null) return DEFAULT_MAX_RESUMPTION_TABS;
        return IntentUtils.safeGetIntExtra(
                intent, EXTRA_MAX_RESUMPTION_TABS, DEFAULT_MAX_RESUMPTION_TABS);
    }

    private static long getTabTtlLimit(@Nullable Intent intent) {
        if (intent == null) return DEFAULT_RESUMPTION_TAB_TTL_MS;
        return IntentUtils.safeGetLongExtra(
                intent, EXTRA_RESUMPTION_TAB_TTL_MS, DEFAULT_RESUMPTION_TAB_TTL_MS);
    }

    private static void saveStateAndPruneDisk(
            TabState state, int tabId, int maxTabs, long ttlMs, CipherFactory cipherFactory) {
        try {
            File dir = getStorageDirectory();
            if (!dir.exists()) {
                if (!dir.mkdirs()) {
                    Log.e(TAG, "saveStateAndPruneDisk: Failed to create storage directory");
                    return;
                }
            }
            TabStateFileManager.saveState(
                    dir, state, tabId, /* isEncrypted= */ true, cipherFactory);
            Log.i(
                    TAG,
                    "saveTabStateDeferred: successfully saved state to disk for tabId: %d",
                    tabId);
            pruneDirectory(dir, maxTabs, ttlMs);
        } catch (Exception e) {
            Log.e(TAG, "saveTabStateDeferred: Exception during saveState", e);
        }
    }

    private static void pruneDirectory(File dir, int maxFiles, long ttlMs) {
        File[] files = dir.listFiles();
        if (files == null) return;

        long thresholdTime = System.currentTimeMillis() - ttlMs;

        for (File f : files) {
            if (f.lastModified() < thresholdTime) {
                Log.i(TAG, "pruneDirectory: Deleting expired file %s", f.getName());
                if (!f.delete()) {
                    Log.e(TAG, "pruneDirectory: Failed to delete expired file %s", f.getName());
                }
            }
        }

        files = dir.listFiles();
        if (files == null) return;

        if (files.length > maxFiles) {
            Arrays.sort(files, (f1, f2) -> Long.compare(f1.lastModified(), f2.lastModified()));
            int filesToDelete = files.length - maxFiles;
            for (int i = 0; i < filesToDelete; i++) {
                Log.i(
                        TAG,
                        "pruneDirectory: Exceeded quota, deleting oldest file %s",
                        files[i].getName());
                if (!files[i].delete()) {
                    Log.e(
                            TAG,
                            "pruneDirectory: Failed to delete oldest file %s",
                            files[i].getName());
                }
            }
        }
    }

    private static @Nullable TabState cloneTabState(@Nullable TabState original) {
        if (original == null) return null;
        TabState clone = new TabState();
        if (original.contentsState != null) {
            ByteBuffer originalBuffer = original.contentsState.buffer();
            if (originalBuffer != null) {
                ByteBuffer duplicate = originalBuffer.duplicate();
                duplicate.position(0);
                ByteBuffer cloneBuffer = ByteBuffer.allocateDirect(duplicate.remaining());
                cloneBuffer.put(duplicate);
                cloneBuffer.flip();
                clone.contentsState =
                        new WebContentsState(cloneBuffer, original.contentsState.version());
            }
        }
        clone.timestampMillis = original.timestampMillis;
        clone.parentId = original.parentId;
        clone.tabLaunchTypeAtCreation = original.tabLaunchTypeAtCreation;
        clone.themeColor = original.themeColor;
        clone.rootId = original.rootId;
        clone.userAgent = original.userAgent;
        clone.lastNavigationCommittedTimestampMillis =
                original.lastNavigationCommittedTimestampMillis;
        clone.tabGroupId = original.tabGroupId;
        clone.tabHasSensitiveContent = original.tabHasSensitiveContent;
        clone.isPinned = original.isPinned;
        clone.url = original.url;
        return clone;
    }

    private static void requestFocusInternal(View view) {
        if (view.isAttachedToWindow()) {
            view.requestFocus();
        } else {
            view.addOnAttachStateChangeListener(
                    new View.OnAttachStateChangeListener() {
                        @Override
                        public void onViewAttachedToWindow(View v) {
                            v.requestFocus();
                            v.removeOnAttachStateChangeListener(this);
                        }

                        @Override
                        public void onViewDetachedFromWindow(View v) {}
                    });
        }
    }
}
