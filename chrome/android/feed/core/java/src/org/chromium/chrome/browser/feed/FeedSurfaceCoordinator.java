// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ScrollView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.library.api.client.scope.ProcessScope;
import org.chromium.chrome.browser.feed.library.api.client.scope.StreamScope;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarCallbackApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.StreamConfiguration;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.shared.FeedSurfaceDelegate;
import org.chromium.chrome.browser.feed.shared.FeedSurfaceProvider;
import org.chromium.chrome.browser.feed.shared.stream.Header;
import org.chromium.chrome.browser.feed.shared.stream.NonDismissibleHeader;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.feed.tooltip.BasicTooltipApi;
import org.chromium.chrome.browser.feed.v2.FeedStream;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.cards.promo.HomepagePromoController;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderView;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.feed.R;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ViewUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Provides a surface that displays an interest feed rendered list of content suggestions.
 */
public class FeedSurfaceCoordinator implements FeedSurfaceProvider {
    private final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    @Nullable
    private final View mNtpHeader;
    private final ActionApi mActionApi;
    private final boolean mShowDarkBackground;
    private final boolean mIsPlaceholderShown;
    private final FeedSurfaceDelegate mDelegate;
    private final int mDefaultMargin;
    private final int mWideMargin;
    private final FeedSurfaceMediator mMediator;
    private final BottomSheetController mBottomSheetController;

    private UiConfig mUiConfig;
    private FrameLayout mRootView;
    private ContextMenuManager mContextMenuManager;
    private Tracker mTracker;

    // Homepage promo view will be not-null once we have it created, until it is destroyed.
    private @Nullable View mHomepagePromoView;
    private @Nullable HomepagePromoController mHomepagePromoController;

    // Used when Feed is enabled.
    private @Nullable Stream mStream;
    private @Nullable FeedImageLoader mImageLoader;
    private @Nullable StreamLifecycleManager mStreamLifecycleManager;
    private @Nullable SectionHeaderView mSectionHeaderView;
    private @Nullable PersonalizedSigninPromoView mSigninPromoView;
    private @Nullable ViewResizer mStreamViewResizer;
    private @Nullable NativePageNavigationDelegate mPageNavigationDelegate;

    // Used when Feed is disabled by policy.
    private @Nullable ScrollView mScrollViewForPolicy;
    private @Nullable ViewResizer mScrollViewResizer;

    // Used for the feed header menu.
    private UserEducationHelper mUserEducationHelper;

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

    private class SignInPromoHeader implements Header {
        @Override
        public View getView() {
            return getSigninPromoView();
        }

        @Override
        public boolean isDismissible() {
            return true;
        }

        @Override
        public void onDismissed() {
            mMediator.onSignInPromoDismissed();
        }
    }

    private class HomepagePromoHeader implements Header {
        @Override
        public View getView() {
            assert mHomepagePromoView != null;
            return mHomepagePromoView;
        }

        @Override
        public boolean isDismissible() {
            return true;
        }

        @Override
        public void onDismissed() {
            assert mHomepagePromoController != null;
            mHomepagePromoController.dismissPromo();
        }
    }

    /**
     * Provides the additional capabilities needed for the container view.
     */
    private class RootView extends FrameLayout {
        /**
         * @param context The context of the application.
         */
        public RootView(Context context) {
            super(context);
        }

        @Override
        protected void onConfigurationChanged(Configuration newConfig) {
            super.onConfigurationChanged(newConfig);
            mUiConfig.updateDisplayStyle();
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent ev) {
            if (super.onInterceptTouchEvent(ev)) return true;
            if (mMediator != null && !mMediator.getTouchEnabled()) return true;

            return mDelegate.onInterceptTouchEvent(ev);
        }
    }

    /**
     * Provides the additional capabilities needed for the {@link ScrollView}.
     */
    private class PolicyScrollView extends ScrollView {
        public PolicyScrollView(Context context) {
            super(context);
        }

        @Override
        protected void onConfigurationChanged(Configuration newConfig) {
            super.onConfigurationChanged(newConfig);
            mUiConfig.updateDisplayStyle();
        }
    }

    /**
     * Constructs a new FeedSurfaceCoordinator.
     *
     * @param activity The containing {@link ChromeActivity}.
     * @param snackbarManager The {@link SnackbarManager} displaying Snackbar UI.
     * @param tabModelSelector {@link TabModelSelector} object.
     * @param tabProvider Provides the current active tab.
     * @param snapScrollHelper The {@link SnapScrollHelper} for the New Tab Page.
     * @param ntpHeader The extra header on top of the feeds for the New Tab Page.
     * @param sectionHeaderView The {@link SectionHeaderView} for the feed.
     * @param actionApi The {@link ActionApi} implementation to handle actions.
     * @param showDarkBackground Whether is shown on dark background.
     * @param delegate The constructing {@link FeedSurfaceDelegate}.
     * @param pageNavigationDelegate The {@link NativePageNavigationDelegate}
     *                               that handles page navigation.
     * @param profile The current user profile.
     * @param isPlaceholderShown Whether the placeholder should be shown.
     */
    public FeedSurfaceCoordinator(Activity activity, SnackbarManager snackbarManager,
            TabModelSelector tabModelSelector, Supplier<Tab> tabProvider,
            @Nullable SnapScrollHelper snapScrollHelper, @Nullable View ntpHeader,
            @Nullable SectionHeaderView sectionHeaderView, ActionApi actionApi,
            boolean showDarkBackground, FeedSurfaceDelegate delegate,
            @Nullable NativePageNavigationDelegate pageNavigationDelegate, Profile profile,
            boolean isPlaceholderShown, BottomSheetController bottomSheetController) {
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mNtpHeader = ntpHeader;
        mSectionHeaderView = sectionHeaderView;
        mActionApi = actionApi;
        mShowDarkBackground = showDarkBackground;
        mIsPlaceholderShown = isPlaceholderShown;
        mDelegate = delegate;
        mPageNavigationDelegate = pageNavigationDelegate;
        mBottomSheetController = bottomSheetController;

        Resources resources = mActivity.getResources();
        mDefaultMargin =
                resources.getDimensionPixelSize(R.dimen.content_suggestions_card_modern_margin);
        mWideMargin = resources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);

        mRootView = new RootView(mActivity);
        mRootView.setPadding(0, resources.getDimensionPixelOffset(R.dimen.tab_strip_height), 0, 0);
        mUiConfig = new UiConfig(mRootView);

        mTracker = TrackerFactory.getTrackerForProfile(profile);

        // Mediator should be created before any Stream changes.
        mMediator = new FeedSurfaceMediator(this, snapScrollHelper, mPageNavigationDelegate);

        // Add the homepage promo card when the feed is enabled and the feature is enabled. A null
        // mStream object means that the feed is disabled. The intialization of the mStream object
        // is handled during the construction of the FeedSurfaceMediator where there might be cases
        // where the mStream object remains null because the feed is disabled, in which case the
        // homepage promo card cannot be added to the feed even if the feature is enabled.
        if (mStream != null && ChromeFeatureList.isEnabled(ChromeFeatureList.HOMEPAGE_PROMO_CARD)) {
            mHomepagePromoController =
                    new HomepagePromoController(mActivity, mSnackbarManager, mTracker, mMediator);
            mMediator.onHomepagePromoStateChange();
        }

        mUserEducationHelper = new UserEducationHelper(mActivity);
    }

    @Override
    public void destroy() {
        mMediator.destroy();
        if (mStreamLifecycleManager != null) mStreamLifecycleManager.destroy();
        mStreamLifecycleManager = null;
        if (mImageLoader != null) mImageLoader.destroy();
        mImageLoader = null;
        if (mHomepagePromoController != null) mHomepagePromoController.destroy();
    }

    @Override
    public ContextMenuManager.TouchEnabledDelegate getTouchEnabledDelegate() {
        return mMediator;
    }

    @Override
    public NewTabPageLayout.ScrollDelegate getScrollDelegate() {
        return mMediator;
    }

    @Override
    public UiConfig getUiConfig() {
        return mUiConfig;
    }

    @Override
    public View getView() {
        return mRootView;
    }

    @Override
    public boolean shouldCaptureThumbnail() {
        return mMediator.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        ViewUtils.captureBitmap(mRootView, canvas);
        mMediator.onThumbnailCaptured();
    }

    /**
     * @return The {@link StreamLifecycleManager} that manages the lifecycle of the {@link Stream}.
     */
    StreamLifecycleManager getStreamLifecycleManager() {
        return mStreamLifecycleManager;
    }

    /** @return The {@link Stream} that this class holds. */
    Stream getStream() {
        return mStream;
    }

    /**
     * Create a {@link Stream} for this class.
     */
    void createStream() {
        if (mScrollViewForPolicy != null) {
            mRootView.removeView(mScrollViewForPolicy);
            mScrollViewForPolicy = null;
            mScrollViewResizer.detach();
            mScrollViewResizer = null;
        }

        if (FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_V2)) {
            mStream = new FeedStream(mActivity, mShowDarkBackground, mSnackbarManager,
                    mPageNavigationDelegate, mBottomSheetController);
        } else {
            ProcessScope feedProcessScope = FeedProcessScopeFactory.getFeedProcessScope();
            assert feedProcessScope != null;

            FeedAppLifecycle appLifecycle = FeedProcessScopeFactory.getFeedAppLifecycle();
            appLifecycle.onNTPOpened();

            mImageLoader = new FeedImageLoader(
                    mActivity, GlobalDiscardableReferencePool.getReferencePool());
            TooltipApi tooltipApi = new BasicTooltipApi();

            StreamScope streamScope =
                    feedProcessScope
                            .createStreamScopeBuilder(mActivity, mImageLoader, mActionApi,
                                    new BasicStreamConfiguration(),
                                    new BasicCardConfiguration(mActivity.getResources(), mUiConfig),
                                    new BasicSnackbarApi(mSnackbarManager),
                                    FeedProcessScopeFactory.getFeedOfflineIndicator(), tooltipApi)
                            .setIsBackgroundDark(mShowDarkBackground)
                            .setIsPlaceholderShown(mIsPlaceholderShown)
                            .build();

            mStream = streamScope.getStream();
        }

        mStreamLifecycleManager = mDelegate.createStreamLifecycleManager(mStream, mActivity);

        View view = mStream.getView();
        view.setBackgroundResource(R.color.default_bg_color);
        if (mIsPlaceholderShown) {
            // Set recyclerView as transparent until first patch of articles are loaded. Before
            // that, the placeholder is shown.
            view.getBackground().setAlpha(0);
        }
        mRootView.addView(view);
        mStreamViewResizer =
                ViewResizer.createAndAttach(view, mUiConfig, mDefaultMargin, mWideMargin);

        if (mNtpHeader != null) UiUtils.removeViewFromParent(mNtpHeader);
        if (mSectionHeaderView != null) UiUtils.removeViewFromParent(mSectionHeaderView);
        if (mSigninPromoView != null) UiUtils.removeViewFromParent(mSigninPromoView);
        if (mHomepagePromoView != null) UiUtils.removeViewFromParent(mHomepagePromoView);

        if (mNtpHeader != null) {
            mStream.setHeaderViews(Arrays.asList(new NonDismissibleHeader(mNtpHeader),
                    new NonDismissibleHeader(mSectionHeaderView)));
        } else if (mSectionHeaderView != null) {
            mStream.setHeaderViews(Arrays.asList(new NonDismissibleHeader(mSectionHeaderView)));
        }
        mStream.addScrollListener(new FeedLoggingBridge.ScrollEventReporter(
                FeedProcessScopeFactory.getFeedLoggingBridge()));

        // Work around https://crbug.com/943873 where default focus highlight shows up after
        // toggling dark mode.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            view.setDefaultFocusHighlightEnabled(false);
        }

        // Explicitly request focus on the scroll container to avoid UrlBar being focused after
        // the scroll container for policy is removed.
        view.requestFocus();
    }

    /**
     * @return The {@link ScrollView} for displaying content for supervised user or enterprise
     *         policy.
     */
    ScrollView getScrollViewForPolicy() {
        return mScrollViewForPolicy;
    }

    /**
     * Create a {@link ScrollView} for displaying content for supervised user or enterprise policy.
     */
    void createScrollViewForPolicy() {
        if (mStream != null) {
            mStreamViewResizer.detach();
            mStreamViewResizer = null;
            mRootView.removeView(mStream.getView());
            assert mStreamLifecycleManager
                    != null
                : "StreamLifecycleManager should not be null when the Stream is not null.";
            mStreamLifecycleManager.destroy();
            mStreamLifecycleManager = null;
            // Do not call mStream.onDestroy(), the mStreamLifecycleManager has done that for us.
            mStream = null;
            mSectionHeaderView = null;
            mSigninPromoView = null;
            mHomepagePromoView = null;
            // TODO(wenyufu): Support HomepagePromo when policy enabled.
            if (mHomepagePromoController != null) {
                mHomepagePromoController.destroy();
                mHomepagePromoController = null;
            }
            if (mImageLoader != null) {
                mImageLoader.destroy();
                mImageLoader = null;
            }
        }

        mScrollViewForPolicy = new PolicyScrollView(mActivity);
        mScrollViewForPolicy.setBackgroundColor(
                ApiCompatibilityUtils.getColor(mActivity.getResources(), R.color.default_bg_color));
        mScrollViewForPolicy.setVerticalScrollBarEnabled(false);

        // Make scroll view focusable so that it is the next focusable view when the url bar clears
        // focus.
        mScrollViewForPolicy.setFocusable(true);
        mScrollViewForPolicy.setFocusableInTouchMode(true);
        mScrollViewForPolicy.setContentDescription(
                mScrollViewForPolicy.getResources().getString(R.string.accessibility_new_tab_page));

        if (mNtpHeader != null) {
            UiUtils.removeViewFromParent(mNtpHeader);
            mScrollViewForPolicy.addView(mNtpHeader);
        }
        mRootView.addView(mScrollViewForPolicy);
        mScrollViewResizer = ViewResizer.createAndAttach(
                mScrollViewForPolicy, mUiConfig, mDefaultMargin, mWideMargin);
        mScrollViewForPolicy.requestFocus();
    }

    /** @return The {@link SectionHeaderView} for the Feed section header. */
    SectionHeaderView getSectionHeaderView() {
        return mSectionHeaderView;
    }

    /** @return The {@link PersonalizedSigninPromoView} for this class. */
    PersonalizedSigninPromoView getSigninPromoView() {
        if (mSigninPromoView == null) {
            LayoutInflater inflater = LayoutInflater.from(mRootView.getContext());
            mSigninPromoView = (PersonalizedSigninPromoView) inflater.inflate(
                    R.layout.personalized_signin_promo_view_modern_content_suggestions, mRootView,
                    false);
        }
        return mSigninPromoView;
    }

    /** Update header views in the Stream. */
    void updateHeaderViews(boolean isSignInPromoVisible) {
        if (mStream == null) return;

        List<Header> headers = new ArrayList<>();
        if (mNtpHeader != null) {
            assert mSectionHeaderView != null;
            headers.add(new NonDismissibleHeader(mNtpHeader));
        }

        // TODO(wenyufu): check Finch flag for whether sign-in takes precedence over homepage promo
        if (!isSignInPromoVisible && mHomepagePromoController != null) {
            View promoView = mHomepagePromoController.getPromoView();
            if (promoView != null) {
                mHomepagePromoView = promoView;
                headers.add(new HomepagePromoHeader());
            }
        }

        if (mSectionHeaderView != null) {
            headers.add(new NonDismissibleHeader(mSectionHeaderView));
        }

        if (isSignInPromoVisible) {
            headers.add(new SignInPromoHeader());
        }

        mStream.setHeaderViews(headers);
    }

    /**
     * Determines whether the feed header position in the recycler view is suitable for IPH.
     *
     * @param maxPosFraction The maximal fraction of the recycler view height starting from the top
     *                       within which the top position of the feed header can be. The value has
     *                       to be within the range [0.0, 1.0], where at 0.0 the feed header is at
     *                       the very top of the recycler view and at 1.0 is at the very bottom and
     *                       hidden.
     * @return True If the feed header is at a position that is suitable to show the IPH.
     */
    boolean isFeedHeaderPositionInRecyclerViewSuitableForIPH(float maxPosFraction) {
        assert maxPosFraction >= 0.0f
                && maxPosFraction <= 1.0f
            : "Max position fraction should be ranging between 0.0 and 1.0";

        // Get the top position of the section header view in the recycler view.
        int[] headerPositions = new int[2];
        mSectionHeaderView.getLocationOnScreen(headerPositions);
        int topPosInStream = headerPositions[1] - mRootView.getTop();

        if (topPosInStream < 0) return false;
        if (topPosInStream > maxPosFraction * mRootView.getHeight()) return false;

        return true;
    }

    Tracker getFeatureEngagementTracker() {
        return mTracker;
    }

    UserEducationHelper getUserEducationHelper() {
        return mUserEducationHelper;
    }

    @VisibleForTesting
    FeedSurfaceMediator getMediatorForTesting() {
        return mMediator;
    }

    @VisibleForTesting
    public View getSignInPromoViewForTesting() {
        return getSigninPromoView();
    }

    @VisibleForTesting
    public View getSectionHeaderViewForTesting() {
        return getSectionHeaderView();
    }

    @VisibleForTesting
    public Stream getStreamForTesting() {
        return getStream();
    }
}
