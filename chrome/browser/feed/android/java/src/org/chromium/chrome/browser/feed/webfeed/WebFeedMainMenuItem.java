// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator.StreamTabId;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.WebFeedMetadata;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController.FeedLauncher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.LoadingView;
import org.chromium.url.GURL;

/**
 * Specific {@link FrameLayout} that displays the Web Feed footer in the main menu.
 */
public class WebFeedMainMenuItem extends FrameLayout {
    private static final int LOADING_REFRESH_TIME_MS = 400;

    private final Context mContext;

    private GURL mUrl;
    private Tab mTab;
    private String mTitle;
    private AppMenuHandler mAppMenuHandler;

    // Points to the currently shown chip: null, mFollowingChipView, mFollowChipView,
    private ChipView mChipView;

    // Child views, null before inflation.
    private ChipView mFollowingChipView;
    private ChipView mFollowChipView;
    private ImageView mIcon;
    private TextView mItemText;

    private WebFeedFaviconFetcher mFaviconFetcher;
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
        mIcon = findViewById(R.id.icon);
        mFollowingChipView = findViewById(R.id.following_chip_view);
        mFollowChipView = findViewById(R.id.follow_chip_view);
        mItemText = findViewById(R.id.menu_item_text);
    }

    /**
     * Initialize the Web Feed main menu item.
     *
     * @param tab The current {@link Tab}.
     * @param appMenuHandler {@link AppMenuHandler} to control hiding the app menu.
     * @param feedLauncher {@link FeedLauncher}
     * @param largeIconBridge {@link LargeIconBridge} to get the favicon of the page.
     * @param dialogManager {@link ModalDialogManager} for managing the dialog.
     * @param snackbarManager {@link SnackbarManager} to display snackbars.
     */
    public void initialize(Tab tab, AppMenuHandler appMenuHandler,
            WebFeedFaviconFetcher faviconFetcher, FeedLauncher feedLauncher,
            ModalDialogManager dialogManager, SnackbarManager snackbarManager) {
        mUrl = tab.getOriginalUrl();
        mTab = tab;
        mAppMenuHandler = appMenuHandler;
        mFaviconFetcher = faviconFetcher;
        mWebFeedSnackbarController = new WebFeedSnackbarController(
                mContext, feedLauncher, dialogManager, snackbarManager);
        Callback<WebFeedMetadata> metadata_callback = result -> {
            initializeFavicon(result);
            initializeText(result);
            initializeChipView(result);
            if (mChipView != null && mTab.isShowingErrorPage()) {
                mChipView.setEnabled(false);
            }
        };
        WebFeedBridge.getWebFeedMetadataForPage(mTab, mUrl,
                WebFeedPageInformationRequestReason.MENU_ITEM_PRESENTATION, metadata_callback);
    }

    private void initializeFavicon(WebFeedMetadata webFeedMetadata) {
        mFaviconFetcher.beginFetch(
                mContext.getResources().getDimensionPixelSize(R.dimen.web_feed_icon_size),
                mContext.getResources().getDimensionPixelSize(R.dimen.web_feed_monogram_text_size),
                mUrl, webFeedMetadata != null ? webFeedMetadata.faviconUrl : null,
                this::onFaviconFetched);
    }

    private void initializeText(WebFeedMetadata webFeedMetadata) {
        if (webFeedMetadata != null && !TextUtils.isEmpty(webFeedMetadata.title)) {
            mTitle = webFeedMetadata.title;
        } else {
            mTitle = UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(mUrl);
        }
        mItemText.setText(mTitle);
    }

    private void initializeChipView(WebFeedMetadata webFeedMetadata) {
        @WebFeedSubscriptionStatus
        int subscriptionStatus = webFeedMetadata == null ? WebFeedSubscriptionStatus.UNKNOWN
                                                         : webFeedMetadata.subscriptionStatus;
        if (subscriptionStatus == WebFeedSubscriptionStatus.UNKNOWN
                || subscriptionStatus == WebFeedSubscriptionStatus.NOT_SUBSCRIBED) {
            hideCurrentChipAndThen(this::showUnsubscribedChipView);
        } else if (subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBED) {
            hideCurrentChipAndThen(() -> showSubscribedChipView(webFeedMetadata.id));
        } else if (subscriptionStatus == WebFeedSubscriptionStatus.UNSUBSCRIBE_IN_PROGRESS) {
            showLoadingChipView(mFollowingChipView, mContext.getText(R.string.menu_following));
        } else if (subscriptionStatus == WebFeedSubscriptionStatus.SUBSCRIBE_IN_PROGRESS) {
            showLoadingChipView(mFollowChipView, mContext.getText(R.string.menu_follow));
        }
    }

    private void showUnsubscribedChipView() {
        mChipView = mFollowChipView;
        showEnabledChipView(mFollowChipView, mContext.getText(R.string.menu_follow),
                R.drawable.ic_add, (view) -> {
                    WebFeedBridge.followFromUrl(mTab, mUrl, result -> {
                        byte[] followId = result.metadata != null ? result.metadata.id : null;
                        mWebFeedSnackbarController.showPostFollowHelp(
                                mTab, result, followId, mUrl, mTitle);
                        PrefService prefs = FeedFeatures.getPrefService();
                        if (!prefs.getBoolean(Pref.ARTICLES_LIST_VISIBLE)) {
                            prefs.setBoolean(Pref.ARTICLES_LIST_VISIBLE, true);
                            FeedFeatures.setLastSeenFeedTabId(StreamTabId.FOLLOWING);
                        }
                    });
                    mAppMenuHandler.hideAppMenu();
                });
    }

    private void showSubscribedChipView(byte[] webFeedId) {
        mChipView = mFollowingChipView;
        showEnabledChipView(mFollowingChipView, mContext.getText(R.string.menu_following),
                R.drawable.ic_check_googblue_24dp, (view) -> {
                    WebFeedBridge.unfollow(webFeedId, /*isDurable=*/false,
                            (result)
                                    -> mWebFeedSnackbarController.showSnackbarForUnfollow(
                                            result.requestStatus, webFeedId, mUrl, mTitle));
                    mAppMenuHandler.hideAppMenu();
                });
    }

    private void showLoadingChipView(ChipView chipView, CharSequence chipText) {
        mChipView = chipView;
        if (chipView.getVisibility() == View.GONE) {
            TextView chipTextView = chipView.getPrimaryTextView();
            chipTextView.setText(chipText);
            chipView.setEnabled(false);
            chipView.setVisibility(View.INVISIBLE);
            chipView.showLoadingView(new LoadingView.Observer() {
                boolean mCalled;
                @Override
                public void onShowLoadingUIComplete() {
                    if (mCalled) {
                        return;
                    }
                    mCalled = true;
                    chipView.setVisibility(View.VISIBLE);
                }

                @Override
                public void onHideLoadingUIComplete() {}
            });
        }
        postDelayed(()
                            -> WebFeedBridge.getWebFeedMetadataForPage(mTab, mUrl,
                                    WebFeedPageInformationRequestReason.MENU_ITEM_PRESENTATION,
                                    this::initializeChipView),
                LOADING_REFRESH_TIME_MS);
    }

    private void hideCurrentChipAndThen(Runnable afterHidden) {
        if (mChipView != null) {
            ChipView chip = mChipView;
            mChipView.hideLoadingView(new LoadingView.Observer() {
                boolean mCalled;

                @Override
                public void onShowLoadingUIComplete() {}

                @Override
                public void onHideLoadingUIComplete() {
                    if (mCalled) {
                        return;
                    }
                    mCalled = true;
                    chip.setVisibility(View.GONE);
                    afterHidden.run();
                }
            });
        } else {
            afterHidden.run();
        }
    }

    private void showEnabledChipView(ChipView chipView, CharSequence chipText,
            @DrawableRes int chipIconRes, OnClickListener onClickListener) {
        TextView chipTextView = chipView.getPrimaryTextView();
        chipTextView.setText(chipText);
        chipView.setIcon(chipIconRes, /*tintWithTextColor=*/true);
        chipView.setOnClickListener(onClickListener);
        chipView.setEnabled(!mTab.isShowingErrorPage());
        chipView.setVisibility(View.VISIBLE);
    }

    private void onFaviconFetched(Bitmap icon) {
        mIcon.setImageBitmap(icon);
        if (icon == null) {
            mIcon.setVisibility(View.GONE);
        }
    }
}
