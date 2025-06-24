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
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.List;

/** Bridges between the C++ and Java {@link TabModel} interfaces. */
@NullMarked
public abstract class TabModelJniBridge implements TabModelInternal {
    private final Profile mProfile;

    /** Native TabModelJniBridge pointer, which will be set by {@link #initializeNative()}. */
    private long mNativeTabModelJniBridge;

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
                TabModelJniBridgeJni.get()
                        .init(TabModelJniBridge.this, mProfile, activityType, isArchivedTabModel);
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
            TabModelJniBridgeJni.get().destroy(mNativeTabModelJniBridge, TabModelJniBridge.this);
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
    public abstract @Nullable Tab getTabAt(int index);

    @Override
    public Profile getProfile() {
        return mProfile;
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
        TabModelJniBridgeJni.get()
                .broadcastSessionRestoreComplete(mNativeTabModelJniBridge, TabModelJniBridge.this);
    }

    /**
     * Called by subclasses when a Tab is added to the TabModel.
     * @param tab Tab being added to the model.
     */
    protected void tabAddedToModel(Tab tab) {
        if (isNativeInitialized()) {
            TabModelJniBridgeJni.get()
                    .tabAddedToModel(mNativeTabModelJniBridge, TabModelJniBridge.this, tab);
        }
    }

    protected void duplicateTabForTesting(int index) {
        TabModelJniBridgeJni.get()
                .duplicateTabForTesting( // IN-TEST
                        mNativeTabModelJniBridge, TabModelJniBridge.this, index);
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

        // This behavior is safe for existing native callers (devtools, and a few niche features).
        // If this is ever to be used more regularly from native the ability to specify
        // `allowDialog` should be exposed.
        getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTab(tab).allowUndo(false).build(),
                        /* allowDialog= */ false);
        return true;
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
        return getTabCreator(profile.isOffTheRecord())
                        .createTabWithWebContents(
                                parent,
                                webContents,
                                select
                                        ? TabLaunchType.FROM_RECENT_TABS_FOREGROUND
                                        : TabLaunchType.FROM_RECENT_TABS)
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
            case WindowOpenDisposition.NEW_WINDOW: // fall through
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
    private @Nullable Tab createNewTabForDevTools(GURL url, boolean newWindow) {
        LoadUrlParams loadParams = new LoadUrlParams(url);
        @TabLaunchType int launchType = TabLaunchType.FROM_CHROME_UI;
        if (!newWindow
                || MultiWindowUtils.getInstanceCount() >= MultiWindowUtils.getMaxInstances()) {
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
                        /* addTrustedIntentExtras= */ true);

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
    protected abstract List<Tab> getTabsNavigatedInTimeWindow(long beginTimeMs, long endTimeMs);

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
        for (Tab tab : getAllTabs()) {
            if (!tab.isCustomTab()) {
                return;
            }
        }
        getTabCreator(false).launchNtp();
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
    public void openTabProgrammatically(GURL url, int index) {
        LoadUrlParams loadParams = new LoadUrlParams(url);

        getTabCreator(isIncognitoBranded())
                .createNewTab(
                        loadParams,
                        TabLaunchType.FROM_TAB_LIST_INTERFACE,
                        /* parent= */ null,
                        index);
    }

    /**
     * Duplicates the tab at the given index to the next adjacent index. An out-of-bounds index is
     * ignored.
     *
     * @param index Index of the tab to duplicate.
     */
    @CalledByNative
    public void duplicateTab(int index, WebContents webContents) {
        // TODO(crbug.com/415351293): Copy pinned state once implemented.
        Tab tab = getTabAt(index);
        if (tab == null) return;

        getTabCreator()
                .createTabWithWebContents(tab, webContents, TabLaunchType.FROM_TAB_LIST_INTERFACE);
    }

    @CalledByNative
    protected abstract void moveTabToIndex(int index, int newIndex);

    @CalledByNative
    protected abstract Tab[] getAllTabs();

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long init(
                TabModelJniBridge caller,
                @JniType("Profile*") Profile profile,
                @ActivityType int activityType,
                boolean isArchivedTabModel);

        void broadcastSessionRestoreComplete(
                long nativeTabModelJniBridge, TabModelJniBridge caller);

        void destroy(long nativeTabModelJniBridge, TabModelJniBridge caller);

        void tabAddedToModel(long nativeTabModelJniBridge, TabModelJniBridge caller, Tab tab);

        void duplicateTabForTesting( // IN-TEST
                long nativeTabModelJniBridge, TabModelJniBridge caller, int index);
    }
}
