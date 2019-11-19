// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.os.SystemClock;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;

/**
 * Bridges between the C++ and Java {@link TabModel} interfaces.
 */
public abstract class TabModelJniBridge implements TabModel {
    private final boolean mIsIncognito;

    // TODO(dtrainor, simonb): Make these non-static so we don't break if we have multiple instances
    // of chrome running.  Also investigate how this affects document mode.
    private static long sTabSwitchStartTime;
    private static @TabSelectionType int sTabSelectionType;
    private static boolean sTabSwitchLatencyMetricRequired;
    private static boolean sPerceivedTabSwitchLatencyMetricLogged;

    /** Native TabModelJniBridge pointer, which will be set by {@link #initializeNative()}. */
    private long mNativeTabModelJniBridge;

    /**
     * Whether this tab model is part of a tabbed activity.
     * This is consumed by Sync as part of restoring sync data from a previous session.
     */
    private boolean mIsTabbedActivityForSync;

    public TabModelJniBridge(boolean isIncognito, boolean isTabbedActivity) {
        mIsIncognito = isIncognito;
        mIsTabbedActivityForSync = isTabbedActivity;
    }

    /** Initializes the native-side counterpart to this class. */
    protected void initializeNative() {
        assert mNativeTabModelJniBridge == 0;
        mNativeTabModelJniBridge = TabModelJniBridgeJni.get().init(
                TabModelJniBridge.this, mIsIncognito, mIsTabbedActivityForSync);
    }

    /** @return Whether the native-side pointer has been initialized. */
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
        return mIsIncognito;
    }

    @Override
    public Profile getProfile() {
        return TabModelJniBridgeJni.get().getProfileAndroid(
                mNativeTabModelJniBridge, TabModelJniBridge.this);
    }

    /** Broadcast a native-side notification that all tabs are now loaded from storage. */
    public void broadcastSessionRestoreComplete() {
        assert isNativeInitialized();
        TabModelJniBridgeJni.get().broadcastSessionRestoreComplete(
                mNativeTabModelJniBridge, TabModelJniBridge.this);
    }

    /**
     * Called by subclasses when a Tab is added to the TabModel.
     * @param tab Tab being added to the model.
     */
    protected void tabAddedToModel(Tab tab) {
        if (isNativeInitialized()) {
            TabModelJniBridgeJni.get().tabAddedToModel(
                    mNativeTabModelJniBridge, TabModelJniBridge.this, tab);
        }
    }

    /**
     * Sets the TabModel's index.
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
     * Returns a tab creator for this tab model.
     * @param incognito Whether to return an incognito TabCreator.
     */
    protected abstract TabCreator getTabCreator(boolean incognito);

    /**
     * Creates a Tab with the given WebContents.
     * @param parent      The parent tab that creates the new tab.
     * @param incognito   Whether or not the tab is incognito.
     * @param webContents A {@link WebContents} object.
     * @return Whether or not the Tab was successfully created.
     */
    @CalledByNative
    protected abstract boolean createTabWithWebContents(
            Tab parent, boolean incognito, WebContents webContents);

    @CalledByNative
    protected abstract void openNewTab(Tab parent, String url, String initiatorOrigin,
            String extraHeaders, ResourceRequestBody postData, int disposition,
            boolean persistParentage, boolean isRendererInitiated);

    /**
     * Creates a Tab with the given WebContents for DevTools.
     * @param url URL to show.
     */
    @CalledByNative
    protected Tab createNewTabForDevTools(String url) {
        return getTabCreator(false).createNewTab(
                new LoadUrlParams(url), TabLaunchType.FROM_CHROME_UI, null);
    }

    @Override
    @CalledByNative
    public abstract int getCount();

    @Override
    @CalledByNative
    public abstract int index();

    /** @return Whether or not a sync session is currently being restored. */
    @CalledByNative
    protected abstract boolean isSessionRestoreInProgress();

    @CalledByNative
    @Override
    public abstract boolean isCurrentModel();

    /**
     * Register the start of tab switch latency timing. Called when setIndex() indicates a tab
     * switch event.
     * @param type The type of action that triggered the tab selection.
     */
    public static void startTabSwitchLatencyTiming(final @TabSelectionType int type) {
        sTabSwitchStartTime = SystemClock.uptimeMillis();
        sTabSelectionType = type;
        sTabSwitchLatencyMetricRequired = false;
        sPerceivedTabSwitchLatencyMetricLogged = false;
    }

    /**
     * Should be called a visible {@link Tab} gets a frame to render in the browser process.
     * If we don't get this call, we ignore requests to
     * {@link #flushActualTabSwitchLatencyMetric()}.
     */
    public static void setActualTabSwitchLatencyMetricRequired() {
        if (sTabSwitchStartTime <= 0) return;
        sTabSwitchLatencyMetricRequired = true;
    }

    /**
     * Logs the perceived tab switching latency metric.  This will automatically be logged if
     * the actual metric is set and flushed.
     */
    public static void logPerceivedTabSwitchLatencyMetric() {
        if (sTabSwitchStartTime <= 0 || sPerceivedTabSwitchLatencyMetricLogged) return;

        flushTabSwitchLatencyMetric(true);
        sPerceivedTabSwitchLatencyMetricLogged = true;
    }

    /**
     * Flush the latency metric if called after the indication that a frame is ready.
     */
    public static void flushActualTabSwitchLatencyMetric() {
        if (sTabSwitchStartTime <= 0 || !sTabSwitchLatencyMetricRequired) return;
        logPerceivedTabSwitchLatencyMetric();
        flushTabSwitchLatencyMetric(false);

        sTabSwitchStartTime = 0;
        sTabSwitchLatencyMetricRequired = false;
    }

    private static void flushTabSwitchLatencyMetric(boolean perceived) {
        if (sTabSwitchStartTime <= 0) return;
        final long ms = SystemClock.uptimeMillis() - sTabSwitchStartTime;
        switch (sTabSelectionType) {
            case TabSelectionType.FROM_CLOSE:
                TabModelJniBridgeJni.get().logFromCloseMetric(ms, perceived);
                break;
            case TabSelectionType.FROM_EXIT:
                TabModelJniBridgeJni.get().logFromExitMetric(ms, perceived);
                break;
            case TabSelectionType.FROM_NEW:
                TabModelJniBridgeJni.get().logFromNewMetric(ms, perceived);
                break;
            case TabSelectionType.FROM_USER:
                TabModelJniBridgeJni.get().logFromUserMetric(ms, perceived);
                break;
        }
    }

    @NativeMethods
    interface Natives {
        long init(TabModelJniBridge caller, boolean isIncognito, boolean isTabbedActivity);
        Profile getProfileAndroid(long nativeTabModelJniBridge, TabModelJniBridge caller);
        void broadcastSessionRestoreComplete(
                long nativeTabModelJniBridge, TabModelJniBridge caller);
        void destroy(long nativeTabModelJniBridge, TabModelJniBridge caller);
        void tabAddedToModel(long nativeTabModelJniBridge, TabModelJniBridge caller, Tab tab);

        // Methods for tab switch latency metrics.
        void logFromCloseMetric(long ms, boolean perceived);
        void logFromExitMetric(long ms, boolean perceived);
        void logFromNewMetric(long ms, boolean perceived);
        void logFromUserMetric(long ms, boolean perceived);
    }
}
