// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Manager class for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetManager implements Destroyable {
    private final Activity mActivity;
    private final BottomSheetController mBottomSheetController;
    private @Nullable TabBottomSheetWebUi mWebUi;
    private @Nullable TabBottomSheetCoordinator mTabBottomSheetCoordinator;

    /**
     * Constructor.
     *
     * @param activity The current {@link Activity} instance.
     * @param profileSupplier A supplier for the current {@link Profile}.
     * @param windowAndroid The {@link WindowAndroid} for managing window-level operations.
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet.
     */
    public TabBottomSheetManager(
            Activity activity,
            MonotonicObservableSupplier<Profile> profileSupplier,
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController) {
        mActivity = activity;
        mBottomSheetController = bottomSheetController;
        mWebUi = new TabBottomSheetWebUi(activity, profileSupplier.get(), windowAndroid);
    }

    /**
     * Attempts to show the Tab BottomSheet. This method will first verify a set of eligibility
     * conditions (e.g., feature flags, user preferences) by calling an internal check. If all
     * conditions are met, it will attempt to instantiate and display the promo bottom sheet to the
     * user.
     */
    public void tryToShowBottomSheet(
            View toolbarView, View fuseboxView, Callback<Boolean> onBottomSheetShowAttempted) {
        if (TabBottomSheetUtils.isTabBottomSheetEnabled()) {
            if (mTabBottomSheetCoordinator == null) {
                mTabBottomSheetCoordinator =
                        new TabBottomSheetCoordinator(mActivity, mBottomSheetController);
            }
            assumeNonNull(mWebUi).initialize();
            mTabBottomSheetCoordinator.showBottomSheet(
                    toolbarView,
                    assumeNonNull(mWebUi.getWebUiView()),
                    fuseboxView,
                    onBottomSheetShowAttempted);
        } else {
            destroy();
        }
    }

    @Override
    public void destroy() {
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.destroy();
            mTabBottomSheetCoordinator = null;
        }
        if (mWebUi != null) {
            mWebUi.destroy();
            mWebUi = null;
        }
    }

    /* Testing methods */
    public @Nullable TabBottomSheetCoordinator getTabBottomSheetCoordinatorForTesting() {
        return mTabBottomSheetCoordinator;
    }
}
