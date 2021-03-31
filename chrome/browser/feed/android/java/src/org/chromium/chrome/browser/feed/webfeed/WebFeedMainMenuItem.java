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

import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.widget.ChipView;
import org.chromium.url.GURL;

/**
 * Specific {@link FrameLayout} that displays the Web Feed footer in the main menu.
 */
public class WebFeedMainMenuItem extends FrameLayout {
    private final Context mContext;

    private GURL mUrl;
    private String mTitle;
    private AppMenuHandler mAppMenuHandler;
    private ImageView mIcon;
    private LargeIconBridge mLargeIconBridge;
    private WebFeedBridge mWebFeedBridge;
    private WebFeedBridge.FollowedIds mFollowedIds;
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

        // TODO(crbug/1152592): Migrate away from getFollowedIds to getWebFeedMetadata.
        mFollowedIds = mWebFeedBridge.getFollowedIds(mUrl);
        initializeFavicon();
        initializeText();
        initializeChipView();
    }

    private void initializeFavicon() {
        mIcon = findViewById(R.id.icon);
        mLargeIconBridge.getLargeIconForUrl(mUrl,
                mContext.getResources().getDimensionPixelSize(R.dimen.web_feed_icon_size),
                this::onFaviconAvailable);
    }

    private void initializeText() {
        TextView itemText = findViewById(R.id.menu_item_text);
        mTitle = UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(mUrl);
        itemText.setText(mTitle);
        if (mFollowedIds != null) {
            mWebFeedBridge.getWebFeedMetadata(mFollowedIds.webFeedId.getBytes(), result -> {
                if (result != null) {
                    mTitle = result.title;
                    itemText.setText(mTitle);
                }
            });
        }
    }

    private void initializeChipView() {
        ChipView chipView;
        CharSequence chipText;
        @DrawableRes
        int chipIconRes;
        OnClickListener onClickListener;
        // TODO(crbug/1152592): Account for different loading/unknown cases.
        if (mFollowedIds != null) {
            chipView = findViewById(R.id.following_chip_view);
            chipText = mContext.getText(R.string.menu_following);
            chipIconRes = R.drawable.ic_check_googblue_24dp;
            onClickListener = (view) -> {
                mWebFeedBridge.unfollowFake(mFollowedIds.followId.getBytes(), (result) -> {
                    mWebFeedSnackbarController.showSnackbarForUnfollow(
                            result.requestStatus == WebFeedSubscriptionRequestStatus.SUCCESS,
                            mFollowedIds.followId.getBytes(), mUrl, mTitle);
                });
                mAppMenuHandler.hideAppMenu();
            };
        } else {
            chipView = findViewById(R.id.follow_chip_view);
            chipText = mContext.getText(R.string.menu_follow);
            chipIconRes = R.drawable.ic_add;
            onClickListener = (view) -> {
                mWebFeedBridge.followFromUrlFake(mUrl, (result) -> {
                    mWebFeedSnackbarController.showSnackbarForFollow(result, mUrl, mTitle);
                });
                mAppMenuHandler.hideAppMenu();
            };
        }
        TextView chipTextView = chipView.getPrimaryTextView();
        chipTextView.setText(chipText);
        chipView.setIcon(chipIconRes, /*tintWithTextColor=*/true);
        chipView.setOnClickListener(onClickListener);
        chipView.setVisibility(VISIBLE);
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
