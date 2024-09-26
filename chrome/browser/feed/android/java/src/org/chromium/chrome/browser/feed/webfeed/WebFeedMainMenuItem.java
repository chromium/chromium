// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.feed.SingleWebFeedEntryPoint;
import org.chromium.chrome.browser.feed.StreamKind;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator.StreamTabId;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.FollowResults;
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

/** Specific {@link FrameLayout} that displays the Web Feed footer in the main menu. */
public class WebFeedMainMenuItem extends FrameLayout {
    private static final String TAG = "WebFeedMainMenuItem";
    private static final int LOADING_REFRESH_TIME_MS = 400;

    private Context mContext;

    private GURL mUrl;
    private Tab mTab;
    private String mTitle;
    private AppMenuHandler mAppMenuHandler;
    private Class<?> mCreatorActivityClass;

    // Points to the currently shown chip: null, mFollowingChipView, mFollowChipView,
    private ChipView mChipView;

    // Child views, null before inflation.
    private ChipView mFollowingChipView;
    private ChipView mFollowChipView;
    private ImageView mIcon;
    private TextView mItemText;

    private @Nullable byte[] mRecommendedWebFeedName;

    private WebFeedFaviconFetcher mFaviconFetcher;
    private WebFeedSnackbarController mWebFeedSnackbarController;

    /** Constructs a new {@link WebFeedMainMenuItem} with the appropriate context. */
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

        final ColorStateList textColor =
                AppCompatResources.getColorStateList(
                        mContext, R.color.default_text_color_accent1_tint_list);
        mFollowingChipView.getPrimaryTextView().setTextColor(textColor);
        mFollowChipView.getPrimaryTextView().setTextColor(textColor);
        final ColorStateList backgroundColor =
                AppCompatResources.getColorStateList(
                        mContext, R.color.menu_footer_chip_background_list);
        mFollowChipView.setBackgroundTintList(backgroundColor);
    }

    /**
     * Initialize the Web Feed main menu item.
     *
     * @param tab The current {@link Tab}.
     * @param appMenuHandler {@link AppMenuHandler} to control hiding the app menu.
     * @param feedLauncher {@link FeedLauncher}
     * @param dialogManager {@link ModalDialogManager} for managing the dialog.
     * @param snackbarManager {@link SnackbarManager} to display snackbars.
     * @param creatorActivityClass {@link CreatorActivity} for launching the Creator Activity.
     */
    public void initialize(
            Tab tab,
            AppMenuHandler appMenuHandler,
            WebFeedFaviconFetcher faviconFetcher,
            FeedLauncher feedLauncher,
            ModalDialogManager dialogManager,
            SnackbarManager snackbarManager,
            Class<?> creatorActivityClass) {
        mUrl = tab.getOriginalUrl();
        mTab = tab;
        mAppMenuHandler = appMenuHandler;
        mFaviconFetcher = faviconFetcher;
        mWebFeedSnackbarController =
                new WebFeedSnackbarController(
                        mContext, feedLauncher, dialogManager, snackbarManager);
        mCreatorActivityClass = creatorActivityClass;
        Callback<WebFeedMetadata> metadataCallback =
                result -> {
                    initializeFavicon(result);
                    initializeText(result);
                    initializeChipView(result);

                    if (mChipView != null && mTab.isShowingErrorPage()) {
                        mChipView.setEnabled(false);
                    }
                };
        mRecommendedWebFeedName =
                WebFeedRecommendationFollowAcceleratorController.getWebFeedNameIfPageIsRecommended(
                        mTab);
        if (mRecommendedWebFeedName != null) {
            WebFeedBridge.getWebFeedMetadata(mRecommendedWebFeedName, metadataCallback);
        } else {
            WebFeedBridge.getWebFeedMetadataForPage(
                    mTab,
                    mUrl,
                    WebFeedPageInformationRequestReason.MENU_ITEM_PRESENTATION,
                    metadataCallback);
        }
    }

    private void initializeFavicon(@Nullable WebFeedMetadata webFeedMetadata) {
        mFaviconFetcher.beginFetch(
                mContext.getResources().getDimensionPixelSize(R.dimen.web_feed_icon_size),
                mContext.getResources().getDimensionPixelSize(R.dimen.web_feed_monogram_text_size),
                mUrl,
                webFeedMetadata != null ? webFeedMetadata.faviconUrl : null,
                this::onFaviconFetched);
    }

    public void setContextForTest(Context newContext) {
        mContext = newContext;
    }

    private void initializeText(@Nullable WebFeedMetadata webFeedMetadata) {
        if (webFeedMetadata != null && !TextUtils.isEmpty(webFeedMetadata.title)) {
            mTitle = webFeedMetadata.title;
        } else {
            mTitle = UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(mUrl);
        }
        mItemText.setText(mTitle);
        if (WebFeedBridge.isCormorantEnabledForLocale()) {
            mItemText.setContentDescription(
                    mContext.getString(R.string.cormorant_creator_preview, mTitle));
            mItemText.setOnClickListener(
                    (view) -> {
                        PostTask.postTask(TaskTraits.UI_DEFAULT, this::launchCreatorActivity);
                    });
        }
    }

    private void initializeChipView(@Nullable WebFeedMetadata webFeedMetadata) {
        @WebFeedSubscriptionStatus
        int subscriptionStatus =
                webFeedMetadata == null
                        ? WebFeedSubscriptionStatus.UNKNOWN
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
        showEnabledChipView(
                mFollowChipView,
                mContext.getText(R.string.menu_follow),
                R.drawable.ic_add,
                (view) -> {
                    Callback<FollowResults> onFollowComplete =
                            result -> {
                                byte[] followId =
                                        result.metadata != null ? result.metadata.id : null;
                                mWebFeedSnackbarController.showPostFollowHelp(
                                        mTab,
                                        result,
                                        followId,
                                        mUrl,
                                        mTitle,
                                        WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU);
                                PrefService prefs = FeedFeatures.getPrefService(mTab.getProfile());
                                if (!prefs.getBoolean(Pref.ARTICLES_LIST_VISIBLE)) {
                                    prefs.setBoolean(Pref.ARTICLES_LIST_VISIBLE, true);
                                    FeedFeatures.setLastSeenFeedTabId(
                                            mTab.getProfile(), StreamTabId.FOLLOWING);
                                }
                            };
                    if (mRecommendedWebFeedName != null) {
                        WebFeedBridge.followFromId(
                                mRecommendedWebFeedName,
                                /* isDurable= */ false,
                                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU,
                                onFollowComplete);
                    } else {
                        WebFeedBridge.followFromUrl(
                                mTab,
                                mUrl,
                                WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU,
                                onFollowComplete);
                    }
                    WebFeedBridge.incrementFollowedFromWebPageMenuCount();
                    FeedServiceBridge.reportOtherUserAction(
                            StreamKind.UNKNOWN, FeedUserActionType.TAPPED_FOLLOW_BUTTON);
                    mAppMenuHandler.hideAppMenu();
                });
    }

    private void showSubscribedChipView(byte[] webFeedId) {
        mChipView = mFollowingChipView;
        showEnabledChipView(
                mFollowingChipView,
                mContext.getText(R.string.menu_following),
                R.drawable.ic_check_googblue_24dp,
                (view) -> {
                    WebFeedBridge.unfollow(
                            webFeedId,
                            /* isDurable= */ false,
                            WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU,
                            (result) ->
                                    mWebFeedSnackbarController.showSnackbarForUnfollow(
                                            result.requestStatus,
                                            webFeedId,
                                            mTab,
                                            mUrl,
                                            mTitle,
                                            WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU));
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
            chipView.showLoadingView(
                    new LoadingView.Observer() {
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
        postDelayed(
                () ->
                        WebFeedBridge.getWebFeedMetadataForPage(
                                mTab,
                                mUrl,
                                WebFeedPageInformationRequestReason.MENU_ITEM_PRESENTATION,
                                this::initializeChipView),
                LOADING_REFRESH_TIME_MS);
    }

    private void hideCurrentChipAndThen(Runnable afterHidden) {
        if (mChipView != null) {
            ChipView chip = mChipView;
            mChipView.hideLoadingView(
                    new LoadingView.Observer() {
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

    private void showEnabledChipView(
            ChipView chipView,
            CharSequence chipText,
            @DrawableRes int chipIconRes,
            OnClickListener onClickListener) {
        TextView chipTextView = chipView.getPrimaryTextView();
        chipTextView.setText(chipText);
        chipView.setIcon(chipIconRes, /* tintWithTextColor= */ true);
        chipView.setOnClickListener(onClickListener);
        chipView.setEnabled(!mTab.isShowingErrorPage());
        chipView.setVisibility(View.VISIBLE);
    }

    private void onFaviconFetched(Bitmap icon) {
        mIcon.setImageBitmap(icon);
        if (icon == null) {
            mIcon.setVisibility(View.GONE);
        }
        if (WebFeedBridge.isCormorantEnabledForLocale()) {
            mIcon.setOnClickListener(
                    (view) -> {
                        PostTask.postTask(TaskTraits.UI_DEFAULT, this::launchCreatorActivity);
                    });
        }
    }

    private void launchCreatorActivity() {
        try {
            // Launch a new activity for the creator page.
            Intent intent = new Intent(mContext, mCreatorActivityClass);
            if (mRecommendedWebFeedName != null) {
                intent.putExtra(
                        CreatorIntentConstants.CREATOR_WEB_FEED_ID, mRecommendedWebFeedName);
            }
            intent.putExtra(CreatorIntentConstants.CREATOR_URL, mUrl.getSpec());
            intent.putExtra(
                    CreatorIntentConstants.CREATOR_ENTRY_POINT, SingleWebFeedEntryPoint.MENU);
            intent.putExtra(
                    CreatorIntentConstants.CREATOR_FOLLOWING, mChipView == mFollowingChipView);
            intent.putExtra(CreatorIntentConstants.CREATOR_TAB_ID, mTab.getId());
            mContext.startActivity(intent);
        } catch (Exception e) {
            Log.d(TAG, "Failed to launch CreatorActivity " + e);
        }
    }
}
