// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.feed.FeedV1ActionOptions;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;

/**
 * Wraps management of the Feed V1 stream.
 */
public class FeedStreamWrapper implements FeedSurfaceCoordinator.StreamWrapper {
    private @Nullable FeedImageLoader mImageLoader;
    private Stream mStream;

    public FeedStreamWrapper() {}

    @Override
    public int defaultMarginPixels(Activity activity) {
        return activity.getResources().getDimensionPixelSize(
                R.dimen.content_suggestions_card_modern_margin);
    }

    @Override
    public int wideMarginPixels(Activity activity) {
        return activity.getResources().getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);
    }

    @Override
    public Stream createStream(Profile profile, Activity activity, boolean showDarkBackground,
            SnackbarManager snackbarManager, NativePageNavigationDelegate pageNavigationDelegate,
            UiConfig uiConfig, boolean placeholderShown,
            BottomSheetController bottomSheetController, Supplier<Tab> tabSupplier,
            FeedV1ActionOptions v1ActionOptions) {
        FeedAppLifecycle appLifecycle = FeedProcessScopeFactory.getFeedAppLifecycle();
        appLifecycle.onNTPOpened();

        mImageLoader =
                new FeedImageLoader(activity, GlobalDiscardableReferencePool.getReferencePool());

        ActionApi actionApi = new FeedActionHandler(v1ActionOptions, pageNavigationDelegate,
                FeedProcessScopeFactory.getFeedConsumptionObserver(),
                FeedProcessScopeFactory.getFeedLoggingBridge(), activity, profile);
        mStream = FeedV1StreamCreator.createStream(activity, mImageLoader, actionApi, uiConfig,
                snackbarManager, showDarkBackground, placeholderShown);
        return mStream;
    }

    @Override
    public boolean isPlaceholderShown() {
        return mStream.isPlaceholderShown();
    }

    @Override
    public void doneWithStream() {
        if (mImageLoader != null) {
            mImageLoader.destroy();
            mImageLoader = null;
        }
    }
    @Override
    public void addScrollListener() {
        mStream.addScrollListener(new FeedLoggingBridge.ScrollEventReporter(
                FeedProcessScopeFactory.getFeedLoggingBridge()));
    }
}
