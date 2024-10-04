// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/** Bridges between the C++ and Java {@link TabModel} interfaces. */
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
            @NonNull Profile profile, @ActivityType int activityType, boolean isArchivedTabModel) {
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
    public Profile getProfile() {
        return mProfile;
    }

    /** Broadcast a native-side notification that all tabs are now loaded from storage. */
    public void broadcastSessionRestoreComplete() {
        assert isNativeInitialized();
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

    @Override
    @CalledByNative
    public abstract Tab getTabAt(int index);

    /**
     * Closes the Tab at a particular index.
     * @param index Index of the tab to close.
     * @return Whether the was successfully closed.
     */
    @CalledByNative
    protected abstract boolean closeTabAt(int index);

    /**
     * Returns a tab creator for this {@link TabModel}.
     *
     * Please note that, the {@link TabCreator} and {@TabModelImpl} are separate instances for
     * {@link ChromeTabbedActivity} and {@link CustomTabActivity} across both regular and Incognito
     * modes which allows us to pass the boolean directly.
     *
     * @param incognito A boolean to indicate whether to return IncognitoTabCreator or
     *         RegularTabCreator.
     */
    protected abstract TabCreator getTabCreator(boolean incognito);

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
    protected abstract void openNewTab(
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
     * @param url URL to show.
     */
    @CalledByNative
    protected Tab createNewTabForDevTools(GURL url) {
        return getTabCreator(/* incognito= */ false)
                .createNewTab(new LoadUrlParams(url), TabLaunchType.FROM_CHROME_UI, null);
    }

    /** Returns whether supplied Tab instance has been grouped together with other Tabs. */
    @CalledByNative
    @VisibleForTesting
    static boolean isTabInTabGroup(@NonNull Tab tab) {
        final TabModelFilter filter = TabModelUtils.getTabModelFilterByTab(tab);
        if (filter == null) return false;

        assert filter instanceof TabGroupModelFilter;
        final TabGroupModelFilter groupingFilter = (TabGroupModelFilter) filter;

        return groupingFilter.isTabInTabGroup(tab);
    }

    @Override
    @CalledByNative
    public abstract int getCount();

    @Override
    @CalledByNative
    public abstract int index();

    /** Returns whether or not a sync session is currently being restored. */
    @CalledByNative
    protected abstract boolean isSessionRestoreInProgress();

    @CalledByNative
    @Override
    public abstract boolean isActiveModel();

    @Override
    public abstract void setActive(boolean active);

    @Override
    @CalledByNative
    public abstract int getTabCountNavigatedInTimeWindow(long beginTimeMs, long endTimeMs);

    @Override
    @CalledByNative
    public abstract void closeTabsNavigatedInTimeWindow(long beginTimeMs, long endTimeMs);

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long init(
                TabModelJniBridge caller,
                @JniType("Profile*") Profile profile,
                @ActivityType int activityType,
                boolean trackInNativeModelList);

        void broadcastSessionRestoreComplete(
                long nativeTabModelJniBridge, TabModelJniBridge caller);

        void destroy(long nativeTabModelJniBridge, TabModelJniBridge caller);

        void tabAddedToModel(long nativeTabModelJniBridge, TabModelJniBridge caller, Tab tab);
    }
}
