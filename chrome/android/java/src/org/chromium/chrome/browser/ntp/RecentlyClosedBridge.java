// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;

/**
 * This class allows Java code to get and clear the list of recently closed tabs.
 */
public class RecentlyClosedBridge implements RecentlyClosedTabManager {
    private long mNativeBridge;

    @Nullable
    private Runnable mTabsUpdatedRunnable;

    @CalledByNative
    private static void pushTab(
            List<RecentlyClosedTab> tabs, int id, String title, String url) {
        RecentlyClosedTab tab = new RecentlyClosedTab(id, title, url);
        tabs.add(tab);
    }

    /**
     * Initializes this class with the given profile.
     * @param profile The Profile whose recently closed tabs will be queried.
     */
    public RecentlyClosedBridge(Profile profile) {
        mNativeBridge = RecentlyClosedBridgeJni.get().init(RecentlyClosedBridge.this, profile);
    }

    @Override
    public void destroy() {
        assert mNativeBridge != 0;
        RecentlyClosedBridgeJni.get().destroy(mNativeBridge, RecentlyClosedBridge.this);
        mNativeBridge = 0;
        mTabsUpdatedRunnable = null;
    }

    @Override
    public void setTabsUpdatedRunnable(@Nullable Runnable runnable) {
        mTabsUpdatedRunnable = runnable;
    }

    @Override
    public List<RecentlyClosedTab> getRecentlyClosedTabs(int maxTabCount) {
        List<RecentlyClosedTab> tabs = new ArrayList<RecentlyClosedTab>();
        boolean received = RecentlyClosedBridgeJni.get().getRecentlyClosedTabs(
                mNativeBridge, RecentlyClosedBridge.this, tabs, maxTabCount);
        return received ? tabs : null;
    }

    @Override
    public boolean openRecentlyClosedTab(
            Tab tab, RecentlyClosedTab recentTab, int windowOpenDisposition) {
        return RecentlyClosedBridgeJni.get().openRecentlyClosedTab(
                mNativeBridge, RecentlyClosedBridge.this, tab, recentTab.id, windowOpenDisposition);
    }

    @Override
    public void openRecentlyClosedTab() {
        RecentlyClosedBridgeJni.get().openMostRecentlyClosedTab(
                mNativeBridge, RecentlyClosedBridge.this);
    }

    @Override
    public void clearRecentlyClosedTabs() {
        RecentlyClosedBridgeJni.get().clearRecentlyClosedTabs(
                mNativeBridge, RecentlyClosedBridge.this);
    }

    /**
     * This method will be called every time the list of recently closed tabs is updated.
     */
    @CalledByNative
    private void onUpdated() {
        if (mTabsUpdatedRunnable != null) mTabsUpdatedRunnable.run();
    }

    @NativeMethods
    interface Natives {
        long init(RecentlyClosedBridge caller, Profile profile);
        void destroy(long nativeRecentlyClosedTabsBridge, RecentlyClosedBridge caller);
        boolean getRecentlyClosedTabs(long nativeRecentlyClosedTabsBridge,
                RecentlyClosedBridge caller, List<RecentlyClosedTab> tabs, int maxTabCount);
        boolean openRecentlyClosedTab(long nativeRecentlyClosedTabsBridge,
                RecentlyClosedBridge caller, Tab tab, int recentTabId, int windowOpenDisposition);
        boolean openMostRecentlyClosedTab(
                long nativeRecentlyClosedTabsBridge, RecentlyClosedBridge caller);
        void clearRecentlyClosedTabs(
                long nativeRecentlyClosedTabsBridge, RecentlyClosedBridge caller);
    }
}
