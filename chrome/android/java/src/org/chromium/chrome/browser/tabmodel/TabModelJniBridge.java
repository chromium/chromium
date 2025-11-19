// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Intent;

import androidx.annotation.CallSuper;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Bridges between the C++ and Java {@link TabModel} interfaces. */
@NullMarked
public abstract class TabModelJniBridge implements TabModelInternal {
    private final Profile mProfile;

    /** Native TabModelJniBridge pointer, which will be set by {@link #initializeNative()}. */
    private long mNativeTabModelJniBridge;

    /** Native AndroidBrowserWindow pointer. */
    private long mNativeAndroidBrowserWindow;

    /**
     * @param profile The profile this TabModel belongs to.
     */
    public TabModelJniBridge(Profile profile) {
        mProfile = profile;
    }

    /**
     * Initializes the native-side counterpart to this class.
     *
     * @param activityType The type of activity this TabModel was created in.
     * @param isArchivedTabModel Whether this tab model is for archived tabs. When true, excludes
     *     the model from broadcasting sync updates.
     */
    @CallSuper
    protected void initializeNative(@ActivityType int activityType, boolean isArchivedTabModel) {
        assert mNativeTabModelJniBridge == 0;
        mNativeTabModelJniBridge =
                TabModelJniBridgeJni.get().init(this, mProfile, activityType, isArchivedTabModel);
    }

    /** Returns whether the native-side pointer has been initialized. */
    public boolean isNativeInitialized() {
        return mNativeTabModelJniBridge != 0;
    }

    @Override
    @CallSuper
    public void destroy() {
        if (isNativeInitialized()) {
            // This will invalidate all other native references to this object in child classes.
            TabModelJniBridgeJni.get().destroy(mNativeTabModelJniBridge);
            mNativeTabModelJniBridge = 0;
        }
    }

    @Override
    public boolean isIncognito() {
        return mProfile.isOffTheRecord();
    }

    @Override
    public boolean isOffTheRecord() {
        return mProfile.isOffTheRecord();
    }

    @Override
    public boolean isIncognitoBranded() {
        return mProfile.isIncognitoBranded();
    }

    @Override
    @CalledByNative
    public abstract int index();

    @Override
    @CalledByNative
    public abstract int getCount();

    @Override
    @CalledByNative
    public abstract @JniType("TabAndroid*") @Nullable Tab getTabAt(int index);

    @Override
    public Profile getProfile() {
        return mProfile;
    }

    @Override
    public void associateWithBrowserWindow(long nativeAndroidBrowserWindow) {
        // Ensure this isn't set multiple times.
        assert mNativeAndroidBrowserWindow == 0;
        mNativeAndroidBrowserWindow = nativeAndroidBrowserWindow;

        assert nativeAndroidBrowserWindow != 0;
        TabModelJniBridgeJni.get()
                .associateWithBrowserWindow(mNativeTabModelJniBridge, nativeAndroidBrowserWindow);
    }

    @CalledByNative
    @Override
    public abstract boolean isActiveModel();

    /** Returns whether the model is done initializing itself and should be used in native. */
    public abstract boolean isInitializationComplete();

    /**
     * Required to be called before this object is ready for most usage. Used to indicate all tabs
     * have been loaded and native is ready. This is only called for non-Incognito tab models.
     */
    public abstract void completeInitialization();

    @Override
    public void broadcastSessionRestoreComplete() {
        assert isNativeInitialized();
        assert isInitializationComplete();
        TabModelJniBridgeJni.get().broadcastSessionRestoreComplete(mNativeTabModelJniBridge);
    }

    @Override
    public @Nullable Integer getNativeSessionIdForTesting() {
        if (!isNativeInitialized()) {
            return null;
        }

        return TabModelJniBridgeJni.get().getSessionIdForTesting(mNativeTabModelJniBridge);
    }

    @Override
    public Tab duplicateTab(Tab tab) {
        return TabModelJniBridgeJni.get().duplicateTab(mNativeTabModelJniBridge, tab);
    }

    /**
     * Called by subclasses when a Tab is added to the TabModel.
     *
     * @param tab Tab being added to the model.
     */
    protected void tabAddedToModel(Tab tab) {
        if (isNativeInitialized()) {
            TabModelJniBridgeJni.get().tabAddedToModel(mNativeTabModelJniBridge, tab);
        }
    }

    protected void moveTabToWindowForTesting(
            Tab tab, long nativeAndroidBrowserWindow, int newIndex) {
        TabModelJniBridgeJni.get()
                .moveTabToWindowForTesting( // IN-TEST
                        mNativeTabModelJniBridge, tab, nativeAndroidBrowserWindow, newIndex);
    }

    protected void moveTabGroupToWindowForTesting(
            Token tabGroupId, long nativeAndroidBrowserWindow, int newIndex) {
        TabModelJniBridgeJni.get()
                .moveTabGroupToWindowForTesting( // IN-TEST
                        mNativeTabModelJniBridge, tabGroupId, nativeAndroidBrowserWindow, newIndex);
    }

    /**
     * Sets the TabModel's index.
     *
     * @param index Index of the Tab to select.
     */
    @CalledByNative
    private void setIndex(int index) {
        TabModelUtils.setIndex(this, index);
    }

    /**
     * Closes all tabs. This bypasses protections for shared tab groups where placeholder tabs are
     * created to ensure collaboration data is not destroyed. Prefer {@link #closeTabAt()} to ensure
     * collaboration data is not destroyed by mistake. This is primarily intended for test usage
     * where the loss of collaboration data is acceptable.
     */
    @CalledByNative
    private void forceCloseAllTabs() {
        // Tests need to use forceCloseTabs here. If a native test has left a shared tab group open
        // the protections of TabRemover#closeTabs will kick in and when trying to close all tabs
        // and we won't actually close all tabs.
        getTabRemover().forceCloseTabs(TabClosureParams.closeAllTabs().build());
        commitAllTabClosures();
    }

    /**
     * Closes the Tab at a particular index.
     *
     * @param index Index of the tab to close.
     * @return Whether the was successfully closed.
     */
    @CalledByNative
    private boolean closeTabAt(int index) {
        Tab tab = getTabAt(index);
        if (tab == null) return false;

        closeTab(tab);
        return true;
    }

    /**
     * Closes the given Tab.
     *
     * @param tab The {@link Tab} to close.
     */
    @CalledByNative
    private void closeTab(@JniType("TabAndroid*") Tab tab) {
        // This behavior is safe for existing native callers (devtools, and a few niche features).
        // If this is ever to be used more regularly from native the ability to specify
        // `allowDialog` should be exposed.
        getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTab(tab).allowUndo(false).build(),
                        /* allowDialog= */ false);
    }

    /**
     * Returns the {@link TabCreator} for the given {@link Profile}.
     *
     * <p>Please note that, the {@link TabCreator} and {@TabModelImpl} are separate instances for
     * {@link ChromeTabbedActivity} and {@link CustomTabActivity} across both regular and Incognito
     * modes which allows us to pass the boolean directly.
     *
     * @param incognito A boolean to indicate whether to return IncognitoTabCreator or
     *     RegularTabCreator.
     */
    protected abstract TabCreator getTabCreator(boolean isIncognito);

    /**
     * Creates a Tab with the given WebContents.
     *
     * @param parent The parent tab that creates the new tab.
     * @param profile The profile for which to create the new tab.
     * @param webContents A {@link WebContents} object.
     * @param select Select the created tab.
     * @return Whether or not the Tab was successfully created.
     */
    @CalledByNative
    private boolean createTabWithWebContents(
            Tab parent, Profile profile, WebContents webContents, boolean select) {
        @TabLaunchType
        int type =
                select ? TabLaunchType.FROM_RECENT_TABS_FOREGROUND : TabLaunchType.FROM_RECENT_TABS;
        return getTabCreator(profile.isOffTheRecord())
                        .createTabWithWebContents(
                                parent,
                                /* shouldPin= */ false,
                                webContents,
                                type,
                                webContents.getVisibleUrl(),
                                /* addTabToModel= */ true)
                != null;
    }

    @CalledByNative
    @VisibleForTesting
    public void openNewTab(
            Tab parent,
            GURL url,
            @Nullable Origin initiatorOrigin,
            @JniType("std::string") String extraHeaders,
            ResourceRequestBody postData,
            int disposition,
            boolean persistParentage,
            boolean isRendererInitiated) {
        if (parent.isClosing()) return;

        boolean incognito = parent.isIncognito();
        @TabLaunchType int tabLaunchType = TabLaunchType.FROM_LONGPRESS_FOREGROUND;

        switch (disposition) {
            case WindowOpenDisposition.NEW_WINDOW:
                tabLaunchType = TabLaunchType.FROM_LINK_CREATING_NEW_WINDOW;
                break;
            case WindowOpenDisposition.NEW_FOREGROUND_TAB:
                tabLaunchType =
                        parent.getTabGroupId() == null
                                ? TabLaunchType.FROM_LONGPRESS_FOREGROUND
                                : TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP;
                break;
            case WindowOpenDisposition.NEW_POPUP: // fall through
            case WindowOpenDisposition.NEW_BACKGROUND_TAB:
                tabLaunchType =
                        parent.getTabGroupId() == null
                                ? TabLaunchType.FROM_LONGPRESS_BACKGROUND
                                : TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP;
                break;
            case WindowOpenDisposition.OFF_THE_RECORD:
                incognito = true;
                break;
            default:
                assert false;
        }

        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setInitiatorOrigin(initiatorOrigin);
        loadUrlParams.setVerbatimHeaders(extraHeaders);
        loadUrlParams.setPostData(postData);
        loadUrlParams.setIsRendererInitiated(isRendererInitiated);
        getTabCreator(incognito)
                .createNewTab(loadUrlParams, tabLaunchType, persistParentage ? parent : null);
    }

    /**
     * Creates a Tab with the given WebContents for DevTools.
     *
     * @param url URL to show.
     * @param newWindow Whether to open the new tab in a new window.
     * @return The created tab or null if the tab could not be created.
     */
    @CalledByNative
    private @JniType("TabAndroid*") @Nullable Tab createNewTabForDevTools(
            GURL url, boolean newWindow) {
        LoadUrlParams loadParams = new LoadUrlParams(url);
        @TabLaunchType int launchType = TabLaunchType.FROM_CHROME_UI;
        if (!newWindow
                || MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ACTIVE)
                        >= MultiWindowUtils.getMaxInstances()) {
            return assumeNonNull(
                    getTabCreator(/* isIncognito= */ false)
                            .createNewTab(loadParams, launchType, null));
        }

        // Creating a new window is asynchronous on Android, so create a background tab that we can
        // return immediately and reparent it into a new window.
        WarmupManager warmupManager = WarmupManager.getInstance();
        Tab parentTab = TabModelUtils.getCurrentTab(this);
        // WARNING: parentTab could be null if all tabs were closed; however, getting an activity
        // context from this class is infeasible for the remaining code. For now this seems to
        // not be called from a 0-tab state.
        assumeNonNull(parentTab);
        Profile profile = parentTab.getProfile();
        warmupManager.createRegularSpareTab(profile);
        Tab tab = warmupManager.takeSpareTab(profile, /* initiallyHidden= */ false, launchType);
        tab.loadUrl(loadParams);

        MultiInstanceManager.onMultiInstanceModeStarted();
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        parentTab.getContext(),
                        TabWindowManager.INVALID_WINDOW_ID,
                        /* preferNew= */ true,
                        /* openAdjacently= */ true,
                        /* addTrustedIntentExtras= */ true,
                        NewWindowAppSource.OTHER);

        Activity activity = ContextUtils.activityFromContext(parentTab.getContext());

        ReparentingTask.from(tab)
                .begin(
                        activity,
                        intent,
                        /* startActivityOptions= */ null,
                        /* finalizeCallback= */ null);
        return tab;
    }

    /**
     * Returns the list of non-custom tabs that have {@link
     * Tab#getLastNavigationCommittedTimestampMillis()} within the time range [beginTimeMs,
     * endTimeMs).
     */
    @VisibleForTesting
    public List<Tab> getTabsNavigatedInTimeWindow(long beginTimeMs, long endTimeMs) {
        List<Tab> tabList = new ArrayList<>();
        for (Tab tab : this) {
            if (tab.isCustomTab()) continue;

            final long recentNavigationTime = tab.getLastNavigationCommittedTimestampMillis();
            if (recentNavigationTime >= beginTimeMs && recentNavigationTime < endTimeMs) {
                tabList.add(tab);
            }
        }
        return tabList;
    }

    /**
     * Returns the count of non-custom tabs that have a {@link
     * Tab#getLastNavigationCommittedTimestampMillis()} within the time range [beginTimeMs,
     * endTimeMs).
     */
    @CalledByNative
    @VisibleForTesting
    public int getTabCountNavigatedInTimeWindow(long beginTimeMs, long endTimeMs) {
        return getTabsNavigatedInTimeWindow(beginTimeMs, endTimeMs).size();
    }

    /**
     * Closes non-custom tabs that have a {@link Tab#getLastNavigationCommittedTimestampMillis()}
     * within the time range [beginTimeMs, endTimeMs).
     */
    @CalledByNative
    @VisibleForTesting
    public void closeTabsNavigatedInTimeWindow(long beginTimeMs, long endTimeMs) {
        List<Tab> tabsToClose = getTabsNavigatedInTimeWindow(beginTimeMs, endTimeMs);
        if (tabsToClose.isEmpty()) return;

        var params =
                TabClosureParams.closeTabs(tabsToClose)
                        .allowUndo(false)
                        .saveToTabRestoreService(false)
                        .build();

        getTabRemover().closeTabs(params, /* allowDialog= */ false);

        // Open a new tab if all tabs are closed.
        for (Tab tab : this) {
            if (!tab.isCustomTab()) {
                return;
            }
        }
        TabCreatorUtil.launchNtp(getTabCreator(/* isIncognito= */ false));
    }

    /** Returns whether or not a sync session is currently being restored. */
    @CalledByNative
    protected abstract boolean isSessionRestoreInProgress();

    /**
     * Opens a tab programmatically
     *
     * @param url URL to show.
     * @param index Index for the tab, it will ignore if it is invalid.
     */
    @CalledByNative
    @VisibleForTesting
    public @JniType("TabAndroid*") @Nullable Tab openTabProgrammatically(GURL url, int index) {
        LoadUrlParams loadParams = new LoadUrlParams(url);

        return getTabCreator(isIncognitoBranded())
                .createNewTab(
                        loadParams,
                        TabLaunchType.FROM_TAB_LIST_INTERFACE,
                        /* parent= */ null,
                        index);
    }

    /**
     * Duplicates the tab to the next adjacent index.
     *
     * <p>This method is specifically for TabListInterface and it will calculate the next valid
     * adjacent index based on the parent tab.
     *
     * @param parentTab The tab to duplicate.
     * @param webContents The {@link WebContents} for the new tab.
     * @return The new tab, if the duplication succeeded.
     */
    @CalledByNative
    protected @JniType("TabAndroid*") @Nullable Tab duplicateTab(
            @JniType("TabAndroid*") Tab parentTab, WebContents webContents) {
        return getTabCreator()
                .createTabWithWebContents(
                        parentTab,
                        parentTab.getIsPinned(),
                        webContents,
                        TabLaunchType.FROM_TAB_LIST_INTERFACE,
                        webContents.getVisibleUrl(),
                        /* addTabToModel= */ true);
    }

    /**
     * Highlights a given list of tabs and makes one of them the active tab.
     *
     * <p>This operation is destructive; the given {@code tabs} are added to the current
     * multi-selection set after clearing previously selected tabs.
     *
     * @param tabToActivate The {@link Tab} to set as active. Must be present in the {@code tabs}
     *     list.
     * @param tabs The list of {@link Tab}s to highlight. Must not be empty.
     */
    @CalledByNative
    protected void highlightTabs(
            @JniType("TabAndroid*") Tab tabToActivate,
            @JniType("std::vector<TabAndroid*>") List<Tab> tabs) {
        if (!ChromeFeatureList.sAndroidTabHighlighting.isEnabled()) return;
        assert !tabs.isEmpty() : "The provided tab list cannot be empty.";
        assert tabToActivate != null : "tabToActivate cannot be null";
        Set<Integer> tabIds = new HashSet<>();
        for (Tab tab : tabs) tabIds.add(tab.getId());
        assert tabIds.contains(tabToActivate.getId()) : "tabToActivate not found in tab list";
        clearMultiSelection(/* notifyObservers= */ false);
        setIndex(TabModelUtils.getTabIndexById(this, tabToActivate.getId()));
        setTabsMultiSelected(tabIds, /* isSelected= */ true);
    }

    @CalledByNative
    protected abstract void moveTabToIndex(@JniType("TabAndroid*") Tab tab, int newIndex);

    @CalledByNative
    protected abstract void moveGroupToIndex(
            @JniType("base::Token") Token tabGroupId, int newIndex);

    @CalledByNative
    protected abstract @JniType("std::vector<TabAndroid*>") List<Tab> getAllTabs();

    @CalledByNative
    protected abstract @JniType("std::optional<base::Token>") @Nullable Token addTabsToGroup(
            @JniType("std::optional<base::Token>") @Nullable Token tabGroupId,
            @JniType("std::vector<TabAndroid*>") List<Tab> tabs);

    protected abstract TabUngrouper getTabUngrouper();

    @CalledByNative
    protected void ungroup(@JniType("std::vector<TabAndroid*>") List<Tab> tabs) {
        if (tabs.isEmpty()) return;

        getTabUngrouper().ungroupTabs(tabs, /* trailing= */ true, /* allowDialog= */ false);
    }

    @CalledByNative
    protected void pinTab(@JniType("TabAndroid*") Tab tab) {
        @TabId int tabId = tab.getId();
        if (tabId == Tab.INVALID_TAB_ID) return;

        pinTab(tabId, /* showUngroupDialog= */ false);
    }

    @CalledByNative
    protected void unpinTab(@JniType("TabAndroid*") Tab tab) {
        @TabId int tabId = tab.getId();
        if (tabId == Tab.INVALID_TAB_ID) return;

        unpinTab(tabId);
    }

    @CalledByNative
    private void moveTabToWindowInternal(
            @JniType("TabAndroid*") Tab tab, @Nullable Activity activity, int newIndex) {
        if (activity == null) return;
        moveTabToWindow(tab, activity, newIndex);
    }

    protected abstract void moveTabToWindow(
            @JniType("TabAndroid*") Tab tab, Activity activity, int newIndex);

    @CalledByNative
    private void moveTabGroupToWindowInternal(
            @JniType("base::Token") Token tabGroupId, @Nullable Activity activity, int newIndex) {
        if (activity == null) return;
        moveTabGroupToWindow(tabGroupId, activity, newIndex);
    }

    protected abstract void moveTabGroupToWindow(
            @JniType("base::Token") Token tabGroupId, Activity activity, int newIndex);

    @Override
    public int getPinnedTabsCount() {
        // The index of the first non-pinned tab is equivalent to the number of pinned tabs.
        // For example, if there are 3 pinned tabs at indices 0, 1, and 2, the first non-pinned
        // tab will be at index 3. If all tabs are pinned, this will return getCount(). If no
        // tabs are pinned, this will return 0.
        return findFirstNonPinnedTabIndex();
    }

    @Override
    public void setMuteSetting(List<Tab> tabs, boolean mute) {
        TabModelJniBridgeJni.get().setMuteSetting(mNativeTabModelJniBridge, tabs, mute);
    }

    @Override
    public boolean isMuted(Tab tab) {
        WebContents contents = tab.getWebContents();
        if (contents != null) {
            return contents.isAudioMuted();
        }

        GURL url = tab.getUrl();
        String scheme = url.getScheme();
        if (url.isEmpty()
                || UrlConstants.CHROME_SCHEME.equals(scheme)
                || UrlConstants.CHROME_NATIVE_SCHEME.equals(scheme)) {
            // Chrome URLs don't have content settings, so default to false when WebContents are not
            // available.
            return false;
        }

        @ContentSetting
        int soundSetting =
                WebsitePreferenceBridge.getContentSetting(
                        mProfile, ContentSettingsType.SOUND, url, url);
        return soundSetting == ContentSetting.BLOCK;
    }

    @Override
    public int getActivityTypeForTesting() {
        return TabModelJniBridgeJni.get()
                .getActivityTypeForTesting( // IN-TEST
                        mNativeTabModelJniBridge);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long init(
                TabModelJniBridge self,
                @JniType("Profile*") Profile profile,
                @ActivityType int activityType,
                boolean isArchivedTabModel);

        void broadcastSessionRestoreComplete(long nativeTabModelJniBridge);

        void destroy(long nativeTabModelJniBridge);

        void tabAddedToModel(long nativeTabModelJniBridge, @JniType("TabAndroid*") Tab tab);

        void associateWithBrowserWindow(
                long nativeTabModelJniBridge, long nativeAndroidBrowserWindow);

        void setMuteSetting(
                long nativeTabModelJniBridge,
                @JniType("std::vector<TabAndroid*>") List<Tab> tabs,
                boolean mute);

        @JniType("TabAndroid*")
        Tab duplicateTab(long nativeTabModelJniBridge, @JniType("TabAndroid*") Tab tab);

        void moveTabToWindowForTesting( // IN-TEST
                long nativeTabModelJniBridge,
                @JniType("TabAndroid*") Tab tab,
                long nativeAndroidBrowserWindow,
                int newIndex);

        void moveTabGroupToWindowForTesting( // IN-TEST
                long nativeTabModelJniBridge,
                @JniType("base::Token") Token tabGroupId,
                long nativeAndroidBrowserWindow,
                int newIndex);

        int getSessionIdForTesting(long nativeTabModelJniBridge);

        @JniType("chrome::android::ActivityType")
        @ActivityType
        int getActivityTypeForTesting(long nativeTabModelJniBridge);
    }
}
