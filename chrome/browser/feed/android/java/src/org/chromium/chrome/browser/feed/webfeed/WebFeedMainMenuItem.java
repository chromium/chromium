// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.widget.ChipView;
import org.chromium.ui.widget.LoadingView;
import org.chromium.url.GURL;

/**
 * Specific {@link FrameLayout} that displays the Web Feed footer in the main menu.
 */
public class WebFeedMainMenuItem extends FrameLayout {
    private static final int LOADING_REFRESH_TIME_MS = 400;

    private final Context mContext;

    private GURL mUrl;
    private String mTitle;
    private AppMenuHandler mAppMenuHandler;
    private ChipView mChipView;
    private ImageView mIcon;
    private LargeIconBridge mLargeIconBridge;
    private WebFeedBridge mWebFeedBridge;
    private WebFeedSnackbarController mWebFeedSnackbarController;

    /**
     * Constructs a new {@link WebFeedMainMenuItem} with the appropriate context.
     */
    public WebFeedMainMenuItem(Context context, AttributeSet attrs) {
        super(context, attrs);

        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
    }

    /**
     * Initialize the Web Feed main menu item.
     *
     * @param url {@link GURL} of the page.
     * @param appMenuHandler {@link AppMenuHandler} to control hiding the app menu.
     * @param largeIconBridge {@link LargeIconBridge} to get the favicon of the page.
     * @param snackbarManager {@link SnackbarManager} to display snackbars.
     * @param webFeedBridge {@link WebFeedBridge} to display the menu item and follow/unfollow.
     */
    public void initialize(GURL url, AppMenuHandler appMenuHandler, LargeIconBridge largeIconBridge,
            SnackbarManager snackbarManager, WebFeedBridge webFeedBridge) {
        mUrl = url;
        mAppMenuHandler = appMenuHandler;
        mLargeIconBridge = largeIconBridge;
        mWebFeedBridge = webFeedBridge;
        mWebFeedSnackbarController =
                new WebFeedSnackbarController(mContext, snackbarManager, webFeedBridge);

        initializeFavicon();
        mWebFeedBridge.getWebFeedMetadataForPage(mUrl, result -> {
            initializeText(result);
            initializeChipView(result);
        });
    }

    private void initializeFavicon() {
        mIcon = findViewById(R.id.icon);
        mLargeIconBridge.getLargeIconForUrl(mUrl,
                mContext.getResources().getDimensionPixelSize(R.dimen.web_feed_icon_size),
                this::onFaviconAvailable);
    }

    private void initializeText(WebFeedMetadata webFeedMetadata) {
        TextView itemText = findViewById(R.id.menu_item_text);
        if (webFeedMetadata != null && webFeedMetadata.title != null) {
            mTitle = webFeedMetadata.title;
        } else {
            mTitle = UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(mUrl);
        }
        itemText.setText(mTitle);
    }

    private void initializeChipView(WebFeedMetadata webFeedMetadata) {
        int subscriptionStatus = webFeedMetadata == null ? WebFeedSubscriptionStatus.UNKNOWN
                                                         : webFeedMetadata.subscriptionStatus;
        if (subscriptionStatus == WebFeedSubscriptionStatus.UNKNOWN
                || subscriptionStatus == WebFeedSubscriptionStatus.NOT_SUBSCRIBED) {
            if (mChipView == null) {
                showUnsubscribedChipView();
                return;
            }
            mChipView.hideLoadingView(new LoadingView.Observer() {
                @Override
                public void onShowLoadingUIComplete() {}

                @Override
                public void onHideLoadingUIComplete() {
                    mChipView.setVisibility(View.GONE);
                    showUnsubscribedChipView();
                }
            });
        } else if (subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBED) {
            if (mChipView == null) {
                showSubscribedChipView(webFeedMetadata.id);
                return;
            }
            mChipView.hideLoadingView(new LoadingView.Observer() {
                @Override
                public void onShowLoadingUIComplete() {}

                @Override
                public void onHideLoadingUIComplete() {
                    mChipView.setVisibility(View.GONE);
                    showSubscribedChipView(webFeedMetadata.id);
                }
            });
        } else if (subscriptionStatus == WebFeedSubscriptionStatus.UNSUBSCRIBE_IN_PROGRESS) {
            mChipView = findViewById(R.id.following_chip_view);
            showLoadingChipView(mChipView, mContext.getText(R.string.menu_following));
        } else if (subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBE_IN_PROGRESS) {
            mChipView = findViewById(R.id.follow_chip_view);
            showLoadingChipView(mChipView, mContext.getText(R.string.menu_follow));
        }
    }

    private void showUnsubscribedChipView() {
        mChipView = findViewById(R.id.follow_chip_view);
        showEnabledChipView(
                mChipView, mContext.getText(R.string.menu_follow), R.drawable.ic_add, (view) -> {
                    mWebFeedBridge.followFromUrl(mUrl,
                            (result)
                                    -> mWebFeedSnackbarController.showSnackbarForFollow(
                                            result, mUrl, mTitle));
                    mAppMenuHandler.hideAppMenu();
                });
    }

    private void showSubscribedChipView(byte[] webFeedId) {
        mChipView = findViewById(R.id.following_chip_view);
        showEnabledChipView(mChipView, mContext.getText(R.string.menu_following),
                R.drawable.ic_check_googblue_24dp, (view) -> {
                    mWebFeedBridge.unfollow(webFeedId,
                            (result)
                                    -> mWebFeedSnackbarController.showSnackbarForUnfollow(
                                            result.requestStatus
                                                    == WebFeedSubscriptionRequestStatus.SUCCESS,
                                            webFeedId, mUrl, mTitle));
                    mAppMenuHandler.hideAppMenu();
                });
    }

    private void showLoadingChipView(ChipView chipView, CharSequence chipText) {
        if (mChipView.getVisibility() == View.GONE) {
            TextView chipTextView = chipView.getPrimaryTextView();
            chipTextView.setText(chipText);
            chipView.setEnabled(false);
            chipView.setVisibility(View.INVISIBLE);
            chipView.showLoadingView(new LoadingView.Observer() {
                @Override
                public void onShowLoadingUIComplete() {
                    chipView.setVisibility(View.VISIBLE);
                }

                @Override
                public void onHideLoadingUIComplete() {}
            });
        }
        postDelayed(
                ()
                        -> mWebFeedBridge.getWebFeedMetadataForPage(mUrl, this::initializeChipView),
                LOADING_REFRESH_TIME_MS);
    }

    private void showEnabledChipView(ChipView chipView, CharSequence chipText,
            @DrawableRes int chipIconRes, OnClickListener onClickListener) {
        TextView chipTextView = chipView.getPrimaryTextView();
        chipTextView.setText(chipText);
        chipView.setIcon(chipIconRes, /*tintWithTextColor=*/true);
        chipView.setOnClickListener(onClickListener);
        chipView.setEnabled(true);
        chipView.setVisibility(View.VISIBLE);
    }

    /**
     * Passed as the callback to {@link LargeIconBridge#getLargeIconForUrl}.
     */
    private void onFaviconAvailable(@Nullable Bitmap icon, @ColorInt int fallbackColor,
            boolean isColorDefault, @IconType int iconType) {
        // If we didn't get a favicon, generate a monogram instead
        if (icon == null) {
            // TODO(crbug/1152592): Update monogram according to specs.
            RoundedIconGenerator iconGenerator = createRoundedIconGenerator(fallbackColor);
            icon = iconGenerator.generateIconForUrl(mUrl.getSpec());
            mIcon.setImageBitmap(icon);
            // generateIconForUrl() might return null if the URL is empty or the domain cannot be
            // resolved. See https://crbug.com/987101
            if (icon == null) {
                mIcon.setVisibility(View.GONE);
            }
        } else {
            mIcon.setImageBitmap(icon);
        }
    }

    private RoundedIconGenerator createRoundedIconGenerator(@ColorInt int iconColor) {
        Resources resources = mContext.getResources();
        int iconSize = resources.getDimensionPixelSize(R.dimen.web_feed_icon_size);
        int cornerRadius = iconSize / 2;
        int textSize = resources.getDimensionPixelSize(R.dimen.web_feed_monogram_text_size);

        return new RoundedIconGenerator(iconSize, iconSize, cornerRadius, iconColor, textSize);
    }
}
