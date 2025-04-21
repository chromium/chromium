// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.chromium.build.NullUtil.assertNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

import java.util.ArrayList;
import java.util.List;

/**
 * Android implementation for TabGroupSyncDelegate. Owned by native. Internal to {@link
 * TabGroupSyncService} and responsible for being the glue layer between the service and tab model
 * in all windows. TODO(crbug.com/379699409): Finish implementation.
 */
@NullMarked
@JNINamespace("tab_groups")
public class TabGroupSyncDelegate implements TabWindowManager.Observer {

    /** Convenient wrapper to pass dependencies needed by the delegate from chrome layer. */
    public static class Deps {
        /** For accessing tab models across multiple activities. */
        public final TabWindowManager tabWindowManager;

        /**
         * @param tabWindowManager For accessing tab models across multiple activities.
         */
        public Deps(TabWindowManager tabWindowManager) {
            this.tabWindowManager = tabWindowManager;
        }
    }

    private final TabWindowManager mTabWindowManager;

    @CalledByNative
    static TabGroupSyncDelegate create(long nativePtr, TabGroupSyncDelegate.Deps delegateDeps) {
        return new TabGroupSyncDelegate(nativePtr, delegateDeps);
    }

    private TabGroupSyncDelegate(long nativePtr, TabGroupSyncDelegate.Deps delegateDeps) {
        assert nativePtr != 0;
        mTabWindowManager = assertNonNull(delegateDeps.tabWindowManager);
        mTabWindowManager.addObserver(this);
    }

    @CalledByNative
    void destroy() {
        mTabWindowManager.removeObserver(this);
    }

    @CalledByNative
    private int[] getSelectedTabs() {
        // Find selected tabs across all windows.
        List<Integer> selectedTabIdList = new ArrayList<>();
        for (TabModelSelector tabModelSelector : mTabWindowManager.getAllTabModelSelectors()) {
            TabModel tabModel = tabModelSelector.getModel(/* incognito= */ false);
            selectedTabIdList.add(TabModelUtils.getCurrentTabId(tabModel));
        }

        int[] selectedTabIdArray = new int[selectedTabIdList.size()];
        for (int i = 0; i < selectedTabIdList.size(); i++) {
            selectedTabIdArray[i] = selectedTabIdList.get(i);
        }

        return selectedTabIdArray;
    }

    @CalledByNative
    private String getTabTitle(int tabId) {
        for (TabModelSelector tabModelSelector : mTabWindowManager.getAllTabModelSelectors()) {
            TabModel tabModel = tabModelSelector.getModel(/* incognito= */ false);
            Tab tab = tabModel.getTabById(tabId);
            if (tab != null) {
                return tab.getTitle();
            }
        }
        return "";
    }

    @Override
    public void onTabModelSelectorAdded(TabModelSelector selector) {
        // TODO(crbug.com/379699409): Implement.
    }
}
