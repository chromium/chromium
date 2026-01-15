// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import android.content.Context;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;

/**
 * Helper class to manage the conditions for showing the simple tab bottom sheet and triggering it.
 */
@NullMarked
public class TabBottomSheetSimpleManager implements Destroyable {
    private final Context mContext;
    private final TabModel mTabModel;
    private final TabBottomSheetManager mTabBottomSheetManager;
    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                    onDidSelectTab(tab);
                }
            };

    private @Nullable TabBottomSheetToolbar mToolbar;

    /**
     * Constructor.
     *
     * @param context The Android Context.
     * @param tabModel The regular {@link TabModel} for the current session.
     * @param tabBottomSheetManager The {@link TabBottomSheetManager} for showing the promo.
     */
    public TabBottomSheetSimpleManager(
            Context context, TabModel tabModel, TabBottomSheetManager tabBottomSheetManager) {
        mContext = context;
        mTabModel = tabModel;
        mTabBottomSheetManager = tabBottomSheetManager;

        if (TabBottomSheetUtils.isTabBottomSheetEnabled()) {
            mTabModel.addObserver(mTabModelObserver);
            mToolbar = new TabBottomSheetSimpleToolbar(mContext);
        }
    }

    /** Attempts to show the Simple Tab BottomSheet. */
    public void tryToShowBottomSheet() {
        if (mTabBottomSheetManager != null && mToolbar != null) {
            mTabBottomSheetManager.tryToShowBottomSheet(mToolbar);
        } else {
            destroy();
        }
    }

    @Override
    public void destroy() {
        if (mTabModel != null) {
            mTabModel.removeObserver(mTabModelObserver);
        }
    }

    /* Observer logic. */
    private void onDidSelectTab(Tab tab) {
        if (checkConditionsForTab(tab)) {
            tryToShowBottomSheet();
        }
    }

    // Conditions required for the tab to show the bottomsheet.
    private boolean checkConditionsForTab(Tab tab) {
        return tab != null
                && !tab.isIncognitoBranded()
                && UrlUtilities.isNtpUrl(tab.getUrl())
                && !tab.isClosing()
                && !tab.isHidden();
    }
}
