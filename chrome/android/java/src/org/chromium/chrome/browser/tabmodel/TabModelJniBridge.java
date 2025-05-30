// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.CallSuper;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/** Bridges between the C++ and Java {@link TabModel} interfaces. */
@NullMarked
public abstract class TabModelJniBridge implements TabModelInternal {
    private final Profile mProfile;

    /** The type of the Activity for which this tab model works. */
    private final @ActivityType int mActivityType;

    /** Whether the model is for archvied tabs. */
    private final boolean mIsArchivedTabModel;

    /** Native TabModelJniBridge pointer, which will be set by {@link #initializeNative()}. */
    private long mNativeTabModelJniBridge;

    /**
     * @param profile The profile this TabModel belongs to.
     * @param activityType The type of activity this TabModel was created in.
     * @param isArchivedTabModel Whether this tab model is for archived tabs. When true, excludes
     *     the model from broadcasting sync updates.
     */
    public TabModelJniBridge(
            Profile profile, @ActivityType int activityType, boolean isArchivedTabModel) {
        mProfile = profile;
        mActivityType = activityType;
        mIsArchivedTabModel = isArchivedTabModel;
    }

    /** Initializes the native-side counterpart to this class. */
    protected void initializeNative(Profile profile) {
        assert mNativeTabModelJniBridge == 0;
        mNativeTabModelJniBridge =
                TabModelJniBridgeJni.get()
                        .init(TabModelJniBridge.this, profile, mActivityType, mIsArchivedTabModel);
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

    /** Returns whether the model is done initializing itself and should be used. */
    public abstract boolean isInitializationComplete();

    /**
     * Required to be called before this object is ready for most usage. Used to indicate all tabs
     * have been loaded and native is ready.
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
    protected abstract void forceCloseAllTabs();

    /**
     * Closes the Tab at a particular index.
     *
     * @param index Index of the tab to close.
     * @return Whether the was successfully closed.
     */
    @CalledByNative
    protected abstract boolean closeTabAt(int index);

    /**
     * Creates a Tab with the given WebContents.
     * @param parent      The parent tab that creates the new tab.
     * @param profile     The profile for which to create the new tab.
     * @param webContents A {@link WebContents} object.
     * @param select      Select the created tab.
     * @return Whether or not the Tab was successfully created.
     */
    @CalledByNative
    protected abstract boolean createTabWithWebContents(
            Tab parent, Profile profile, WebContents webContents, boolean select);

    @CalledByNative
    public abstract void openNewTab(
            Tab parent,
            GURL url,
            @Nullable Origin initiatorOrigin,
            @JniType("std::string") String extraHeaders,
            ResourceRequestBody postData,
            int disposition,
            boolean persistParentage,
            boolean isRendererInitiated);

    /**
     * Creates a Tab with the given WebContents for DevTools.
     *
     * @param url URL to show.
     * @param newWindow Whether to open the new tab in a new window.
     * @return The created tab or null if the tab could not be created.
     */
    @CalledByNative
    protected abstract @Nullable Tab createNewTabForDevTools(GURL url, boolean newWindow);

    /**
     * Returns the count of non-custom tabs that have a {@link
     * Tab#getLastNavigationCommittedTimestampMillis()} within the time range [beginTimeMs,
     * endTimeMs).
     */
    @CalledByNative
    protected abstract int getTabCountNavigatedInTimeWindow(long beginTimeMs, long endTimeMs);

    /**
     * Closes non-custom tabs that have a {@link Tab#getLastNavigationCommittedTimestampMillis()}
     * within the time range [beginTimeMs, endTimeMs).
     */
    @CalledByNative
    protected abstract void closeTabsNavigatedInTimeWindow(long beginTimeMs, long endTimeMs);

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
    protected abstract void openTabProgrammatically(GURL url, int index);

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
    }
}
