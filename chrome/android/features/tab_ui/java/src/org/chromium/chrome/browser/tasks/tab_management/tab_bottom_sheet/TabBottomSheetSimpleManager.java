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
                    onDidSelectTab(tab);
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
    public static void tryToShowBottomSheet(Tab tab) {
        TabBottomSheetManager tabBottomSheetManager =
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
        if (tabBottomSheetManager != null) {
            tabBottomSheetManager.tryToShowBottomSheet(
                    /* shouldShowToolbar= */ true, /* shouldShowFusebox */ true);
        }
    }

    @Override
    public void destroy() {
        if (mTabModel != null) {
            mTabModel.removeObserver(mTabModelObserver);
        }
    }

    /* Temp for testing */
    private void onDidSelectTab(Tab tab) {
        if (checkConditionsForTab(tab)) {
            tryToShowBottomSheet(tab);
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
    public static void show(Tab tab) {
        TabBottomSheetManager tabBottomSheetManager =
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
        if (tabBottomSheetManager != null) {
            tabBottomSheetManager.tryToShowBottomSheet(
                    /* shouldShowToolbar= */ true, /* shouldShowFusebox */ true);
        }
    }

    @CalledByNative
    public static void close(Tab tab) {
        TabBottomSheetManager tabBottomSheetManager =
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
        if (tabBottomSheetManager != null) {
            tabBottomSheetManager.tryToCloseBottomSheet();
        }
    }

    @CalledByNative
    public static boolean isOpen(Tab tab) {
        TabBottomSheetManager tabBottomSheetManager =
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
        if (tabBottomSheetManager != null) {
            return tabBottomSheetManager.isSheetShowing();
        } else {
            return false;
        }
    }

    @CalledByNative
    public static void setWebContents(Tab tab, WebContents webContents) {
        TabBottomSheetManager tabBottomSheetManager =
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
        if (tabBottomSheetManager != null) {
            tabBottomSheetManager.setWebContents(webContents);
        }
    }

    @CalledByNative
    public static @Nullable WebContents getWebContents(Tab tab) {
        TabBottomSheetManager tabBottomSheetManager =
                TabBottomSheetUtils.getManagerFromWindow(assumeNonNull(tab.getWindowAndroid()));
        if (tabBottomSheetManager != null) {
            return tabBottomSheetManager.getWebContents();
        } else {
            return null;
        }
    }
}
