// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import android.app.Activity;

import org.chromium.base.CallbackUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.WindowAndroid;

/**
 * Helper class to manage the conditions for showing the simple tab bottom sheet and triggering it.
 */
@NullMarked
public class TabBottomSheetSimpleManager implements Destroyable {
    private final TabModel mTabModel;
    private @Nullable TabBottomSheetFusebox mFusebox;
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
     * @param activity The Android activity.
     * @param tabModel The regular {@link TabModel} for the current session.
     * @param tabBottomSheetManager The {@link TabBottomSheetManager} for showing the promo.
     */
    public TabBottomSheetSimpleManager(
            Activity activity,
            TabModel tabModel,
            MonotonicObservableSupplier<Profile> profileSupplier,
            WindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            SnackbarManager snackbarManager,
            TabBottomSheetManager tabBottomSheetManager) {
        mTabModel = tabModel;
        mTabBottomSheetManager = tabBottomSheetManager;

        if (TabBottomSheetUtils.isTabBottomSheetEnabled()) {
            mTabModel.addObserver(mTabModelObserver);
            mToolbar = new TabBottomSheetSimpleToolbar(activity);
            if (TabBottomSheetUtils.shouldShowFusebox()) {
                mFusebox =
                        new TabBottomSheetFusebox(
                                activity,
                                profileSupplier,
                                windowAndroid,
                                lifecycleDispatcher,
                                CallbackUtils.emptyCallback(),
                                snackbarManager);
            }
        }
    }

    /** Attempts to show the Simple Tab BottomSheet. */
    public void tryToShowBottomSheet() {
        if (mTabBottomSheetManager != null && mToolbar != null) {
            mTabBottomSheetManager.tryToShowBottomSheet(
                    mToolbar.getToolbarView(),
                    mFusebox != null ? mFusebox.getFuseboxView() : null,
                    this::onBottomSheetShowAttempted);
        } else {
            destroy();
        }
    }

    @Override
    public void destroy() {
        if (mTabModel != null) {
            mTabModel.removeObserver(mTabModelObserver);
        }
        if (mFusebox != null) {
            mFusebox.destroy();
            mFusebox = null;
        }
    }

    void onBottomSheetShowAttempted(boolean didSucceed) {
        if (!didSucceed) {
            return;
        }
        if (mFusebox != null) {
            mFusebox.onBottomSheetShown();
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
