// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.WebContents;

/** Interface for native methods to interact with the tab bottom sheet. */
@NullMarked
public class TabBottomSheetSimpleManager implements Destroyable {
    private final TabModel mTabModel;
    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                    onDidSelectTab(tab, lastId);
                }
            };

    /**
     * Constructor.
     *
     * @param tabModel The regular {@link TabModel} for the current session.
     */
    public TabBottomSheetSimpleManager(TabModel tabModel) {
        mTabModel = tabModel;

        // Temp for testing.
        if (TabBottomSheetUtils.isTabBottomSheetEnabled()) {
            mTabModel.addObserver(mTabModelObserver);
        }
    }

    /** Attempts to show the Simple Tab BottomSheet. */
    // Temp for testing.
    public static void tryToShowBottomSheet(Tab tab, int requestId) {
        TabBottomSheetManager tabBottomSheetManager =
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
        if (tabBottomSheetManager != null) {
            tabBottomSheetManager.tryToShowBottomSheet(
                    requestId, /* shouldShowToolbar= */ true, /* shouldShowFusebox= */ true);
        }
    }

    @Override
    public void destroy() {
        if (mTabModel != null) {
            mTabModel.removeObserver(mTabModelObserver);
        }
    }

    /* Temp for testing */
    private void onDidSelectTab(Tab tab, int requestId) {
        if (checkConditionsForTab(tab)) {
            tryToShowBottomSheet(tab, requestId);
        }
    }

    // Temp for testing
    private boolean checkConditionsForTab(Tab tab) {
        return tab != null
                && !tab.isIncognitoBranded()
                && UrlUtilities.isNtpUrl(tab.getUrl())
                && !tab.isClosing()
                && !tab.isHidden();
    }

    // Native calls for glic.
    @CalledByNative
    public static boolean show(Tab tab, int requestId) {
        TabBottomSheetManager tabBottomSheetManager =
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
        if (tabBottomSheetManager != null) {
            return tabBottomSheetManager.tryToShowBottomSheet(
                    requestId, /* shouldShowToolbar= */ true, /* shouldShowFusebox= */ true);
        }
        return false;
    }

    @CalledByNative
    public static void close(Tab tab, int requestId) {
        TabBottomSheetManager tabBottomSheetManager =
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
        if (tabBottomSheetManager != null) {
            tabBottomSheetManager.tryToCloseBottomSheet(requestId);
        }
    }

    @CalledByNative
    public static boolean isOpen(Tab tab, int requestId) {
        TabBottomSheetManager tabBottomSheetManager =
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
        return tabBottomSheetManager != null && tabBottomSheetManager.isSheetShowing(requestId);
    }

    @CalledByNative
    public static boolean setWebContents(Tab tab, @Nullable WebContents webContents) {
        TabBottomSheetManager tabBottomSheetManager =
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
        if (tabBottomSheetManager != null) {
            return tabBottomSheetManager.setWebContents(webContents);
        }
        return false;
    }

    @CalledByNative
    public static @Nullable WebContents getWebContents(Tab tab, int requestId) {
        TabBottomSheetManager tabBottomSheetManager =
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
        return tabBottomSheetManager != null
                ? tabBottomSheetManager.getWebContents(requestId)
                : null;
    }
}
