// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.app.Activity;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.feed.FeedV1ActionOptions;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Wraps management of the Feed V2 stream.
 */
public class FeedStreamWrapper implements FeedSurfaceCoordinator.StreamWrapper {
    private Stream mStream;
    @Override
    public int defaultMarginPixels(Activity activity) {
        return activity.getResources().getDimensionPixelSize(
                R.dimen.content_suggestions_card_modern_margin_v2);
    }

    @Override
    public int wideMarginPixels(Activity activity) {
        return activity.getResources().getDimensionPixelSize(
                R.dimen.ntp_wide_card_lateral_margins_v2);
    }

    @Override
    public Stream createStream(Profile profile, Activity activity, boolean showDarkBackground,
            SnackbarManager snackbarManager, NativePageNavigationDelegate pageNavigationDelegate,
            UiConfig uiConfig, boolean placeholderShown,
            BottomSheetController bottomSheetController, FeedV1ActionOptions v1ActionOptions,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            ModalDialogManager modalDialogManager, Supplier<Tab> tabSupplier,
            Supplier<ContextMenuManager> contextMenuManagerSupplier) {
        mStream = new FeedStream(activity, showDarkBackground, snackbarManager,
                pageNavigationDelegate, bottomSheetController, ephemeralTabCoordinatorSupplier,
                modalDialogManager, tabSupplier, placeholderShown, contextMenuManagerSupplier);
        return mStream;
    }

    @Override
    public boolean isPlaceholderShown() {
        return mStream.isPlaceholderShown();
    }

    @Override
    public void doneWithStream() {}

    @Override
    public void addScrollListener() {}
}
