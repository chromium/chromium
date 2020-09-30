// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.library.api.client.scope.ProcessScope;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarCallbackApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.StreamConfiguration;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.feed.v1.tooltip.BasicTooltipApi;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;

/**
 * Creates a {@link Stream}. This class was created to reduce feed v1 clutter in
 * {@link FeedSurfaceCoordinator}.
 */
class FeedV1StreamCreator {
    /**
     * Create a feed v1 {@link Stream}.
     */
    public static Stream createStream(Activity activity, FeedImageLoader imageLoader,
            ActionApi actionApi, UiConfig uiConfig, SnackbarManager snackbarManager,
            boolean showDarkBackground, boolean isPlaceholderShown) {
        ProcessScope processScope = FeedProcessScopeFactory.getFeedProcessScope();
        assert processScope != null;

        return processScope
                .createStreamScopeBuilder(activity, imageLoader, actionApi,
                        new BasicStreamConfiguration(),
                        new BasicCardConfiguration(activity.getResources(), uiConfig),
                        new BasicSnackbarApi(snackbarManager),
                        FeedProcessScopeFactory.getFeedOfflineIndicator(), new BasicTooltipApi())
                .setIsBackgroundDark(showDarkBackground)
                .setIsPlaceholderShown(isPlaceholderShown)
                .build()
                .getStream();
    }

    private static class BasicSnackbarApi implements SnackbarApi {
        private final SnackbarManager mManager;

        public BasicSnackbarApi(SnackbarManager manager) {
            mManager = manager;
        }

        @Override
        public void show(String message) {
            mManager.showSnackbar(Snackbar.make(message, new SnackbarManager.SnackbarController() {
            }, Snackbar.TYPE_ACTION, Snackbar.UMA_FEED_NTP_STREAM));
        }

        @Override
        public void show(String message, String action, SnackbarCallbackApi callback) {
            mManager.showSnackbar(
                    Snackbar.make(message,
                                    new SnackbarManager.SnackbarController() {
                                        @Override
                                        public void onAction(Object actionData) {
                                            callback.onDismissedWithAction();
                                        }

                                        @Override
                                        public void onDismissNoAction(Object actionData) {
                                            callback.onDismissNoAction();
                                        }
                                    },
                                    Snackbar.TYPE_ACTION, Snackbar.UMA_FEED_NTP_STREAM)
                            .setAction(action, null));
        }
    }

    private static class BasicStreamConfiguration implements StreamConfiguration {
        public BasicStreamConfiguration() {}

        @Override
        public int getPaddingStart() {
            return 0;
        }
        @Override
        public int getPaddingEnd() {
            return 0;
        }
        @Override
        public int getPaddingTop() {
            return 0;
        }
        @Override
        public int getPaddingBottom() {
            return 0;
        }
    }

    private static class BasicCardConfiguration implements CardConfiguration {
        private final Resources mResources;
        private final UiConfig mUiConfig;
        private final int mCornerRadius;
        private final int mCardMargin;
        private final int mCardWideMargin;

        public BasicCardConfiguration(Resources resources, UiConfig uiConfig) {
            mResources = resources;
            mUiConfig = uiConfig;
            mCornerRadius = mResources.getDimensionPixelSize(R.dimen.default_rounded_corner_radius);
            mCardMargin = mResources.getDimensionPixelSize(
                    R.dimen.content_suggestions_card_modern_margin);
            mCardWideMargin =
                    mResources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);
        }

        @Override
        public int getDefaultCornerRadius() {
            return mCornerRadius;
        }

        @Override
        public Drawable getCardBackground() {
            return ApiCompatibilityUtils.getDrawable(mResources,
                    FeedConfiguration.getFeedUiEnabled()
                            ? R.drawable.hairline_border_card_background_with_inset
                            : R.drawable.hairline_border_card_background);
        }

        @Override
        public int getCardBottomMargin() {
            return mCardMargin;
        }

        @Override
        public int getCardStartMargin() {
            return 0;
        }

        @Override
        public int getCardEndMargin() {
            return 0;
        }
    }

    private FeedV1StreamCreator() {}
}
