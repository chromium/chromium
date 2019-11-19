// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.client.scope.ProcessScope;
import com.google.android.libraries.feed.api.client.scope.StreamScope;
import com.google.android.libraries.feed.api.client.stream.Header;
import com.google.android.libraries.feed.api.client.stream.NonDismissibleHeader;
import com.google.android.libraries.feed.api.client.stream.Stream;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.api.host.stream.SnackbarApi;
import com.google.android.libraries.feed.api.host.stream.SnackbarCallbackApi;
import com.google.android.libraries.feed.api.host.stream.StreamConfiguration;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.feed.tooltip.BasicTooltipApi;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationDelegate;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationLayout;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderView;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.widget.displaystyle.UiConfig;
import org.chromium.chrome.browser.ui.widget.displaystyle.ViewResizer;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.feed.R;
import org.chromium.ui.UiUtils;

import java.util.Arrays;

/**
 * Provides a surface that displays an interest feed rendered list of content suggestions.
 */
public class FeedSurfaceCoordinator {
    private final ChromeActivity mActivity;
    @Nullable
    private final View mNtpHeader;
    private final ActionApi mActionApi;
    private final boolean mShowDarkBackground;
    private final FeedSurfaceDelegate mDelegate;
    private final int mDefaultMargin;
    private final int mWideMargin;
    private final FeedSurfaceMediator mMediator;

    private UiConfig mUiConfig;
    private HistoryNavigationLayout mRootView;
    private ContextMenuManager mContextMenuManager;

    // Used when Feed is enabled.
    private @Nullable Stream mStream;
    private @Nullable FeedImageLoader mImageLoader;
    private @Nullable StreamLifecycleManager mStreamLifecycleManager;
    private @Nullable SectionHeaderView mSectionHeaderView;
    private @Nullable PersonalizedSigninPromoView mSigninPromoView;
    private @Nullable ViewResizer mStreamViewResizer;

    // Used when Feed is disabled by policy.
    private @Nullable ScrollView mScrollViewForPolicy;
    private @Nullable ViewResizer mScrollViewResizer;

    /**
     * The delegate of the {@link FeedSurfaceCoordinator} creator needs to implement.
     */
    public interface FeedSurfaceDelegate {
        /**
         * Creates {@link StreamLifecycleManager} for the specified {@link Stream} in the {@link
         * Activity}.
         * @param stream The {@link Stream} managed by the {@link StreamLifecycleManager}.
         * @param activity The associated {@link Activity} of the {@link Stream}.
         * @return The {@link StreamLifecycleManager}.
         */
        StreamLifecycleManager createStreamLifecycleManager(Stream stream, Activity activity);

        /**
         * Checks whether the delegate want to intercept the given touch event.
         * @param ev The given {@link MotioneEvent}
         * @return True if the delegate want to intercept the event, otherwise return false.
         */
        boolean onInterceptTouchEvent(MotionEvent ev);
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

    /**
     * Provides the additional capabilities needed for the container view.
     */
    private class RootView extends HistoryNavigationLayout {
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
     * @param historyNavigationDelegate The {@link HistoryNavigationDelegate} for the root view.
     * @param snapScrollHelper The {@link SnapScrollHelper} for the New Tab Page.
     * @param ntpHeader The extra header on top of the feeds for the New Tab Page.
     * @param sectionHeaderView The {@link SectionHeaderView} for the feed.
     * @param actionApi The {@link ActionApi} implementation to handle actions.
     * @param showDarkBackground Whether is shown on dark background.
     * @param delegate The constructing {@link FeedSurfaceDelegate}.
     */
    public FeedSurfaceCoordinator(ChromeActivity activity,
            @Nullable HistoryNavigationDelegate historyNavigationDelegate,
            @Nullable SnapScrollHelper snapScrollHelper, @Nullable View ntpHeader,
            @Nullable SectionHeaderView sectionHeaderView, ActionApi actionApi,
            boolean showDarkBackground, FeedSurfaceDelegate delegate) {
        mActivity = activity;
        mNtpHeader = ntpHeader;
        mSectionHeaderView = sectionHeaderView;
        mActionApi = actionApi;
        mShowDarkBackground = showDarkBackground;
        mDelegate = delegate;

        Resources resources = mActivity.getResources();
        mDefaultMargin =
                resources.getDimensionPixelSize(R.dimen.content_suggestions_card_modern_margin);
        mWideMargin = resources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);

        mRootView = new RootView(mActivity);
        mRootView.setPadding(0, resources.getDimensionPixelOffset(R.dimen.tab_strip_height), 0, 0);
        if (historyNavigationDelegate != null) {
            mRootView.setNavigationDelegate(historyNavigationDelegate);
        }
        mUiConfig = new UiConfig(mRootView);

        // Mediator should be created before any Stream changes.
        mMediator = new FeedSurfaceMediator(this, snapScrollHelper);
    }

    public void destroy() {
        mMediator.destroy();
        if (mStreamLifecycleManager != null) mStreamLifecycleManager.destroy();
        mStreamLifecycleManager = null;
        if (mImageLoader != null) mImageLoader.destroy();
        mImageLoader = null;
    }

    public ContextMenuManager.TouchEnabledDelegate getTouchEnabledDelegate() {
        return mMediator;
    }

    public NewTabPageLayout.ScrollDelegate getScrollDelegate() {
        return mMediator;
    }

    public UiConfig getUiConfig() {
        return mUiConfig;
    }

    public View getView() {
        return mRootView;
    }

    public boolean shouldCaptureThumbnail() {
        return mMediator.shouldCaptureThumbnail();
    }

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

        ProcessScope feedProcessScope = FeedProcessScopeFactory.getFeedProcessScope();
        assert feedProcessScope != null;

        FeedAppLifecycle appLifecycle = FeedProcessScopeFactory.getFeedAppLifecycle();
        appLifecycle.onNTPOpened();

        mImageLoader =
                new FeedImageLoader(mActivity, GlobalDiscardableReferencePool.getReferencePool());
        TooltipApi tooltipApi = new BasicTooltipApi();

        StreamScope streamScope =
                feedProcessScope
                        .createStreamScopeBuilder(mActivity, mImageLoader, mActionApi,
                                new BasicStreamConfiguration(),
                                new BasicCardConfiguration(mActivity.getResources(), mUiConfig),
                                new BasicSnackbarApi(mActivity.getSnackbarManager()),
                                FeedProcessScopeFactory.getFeedOfflineIndicator(), tooltipApi)
                        .setIsBackgroundDark(mShowDarkBackground)
                        .build();

        mStream = streamScope.getStream();
        mStreamLifecycleManager = mDelegate.createStreamLifecycleManager(mStream, mActivity);

        View view = mStream.getView();
        view.setBackgroundResource(R.color.modern_primary_color);
        mRootView.addView(view);
        mStreamViewResizer =
                ViewResizer.createAndAttach(view, mUiConfig, mDefaultMargin, mWideMargin);

        if (mNtpHeader != null) UiUtils.removeViewFromParent(mNtpHeader);
        if (mSectionHeaderView != null) UiUtils.removeViewFromParent(mSectionHeaderView);
        if (mSigninPromoView != null) UiUtils.removeViewFromParent(mSigninPromoView);

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
            if (mImageLoader != null) {
                mImageLoader.destroy();
                mImageLoader = null;
            }
        }

        mScrollViewForPolicy = new PolicyScrollView(mActivity);
        mScrollViewForPolicy.setBackgroundColor(Color.WHITE);
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
    void updateHeaderViews(boolean isPromoVisible) {
        if (mNtpHeader != null) {
            assert mSectionHeaderView != null;
            mStream.setHeaderViews(
                    isPromoVisible ? Arrays.asList(new NonDismissibleHeader(mNtpHeader),
                            new NonDismissibleHeader(mSectionHeaderView), new SignInPromoHeader())
                                   : Arrays.asList(new NonDismissibleHeader(mNtpHeader),
                                           new NonDismissibleHeader(mSectionHeaderView)));
        } else if (mSectionHeaderView == null) {
            if (isPromoVisible) mStream.setHeaderViews(Arrays.asList(new SignInPromoHeader()));
        } else {
            mStream.setHeaderViews(isPromoVisible
                            ? Arrays.asList(new NonDismissibleHeader(mSectionHeaderView),
                                    new SignInPromoHeader())
                            : Arrays.asList(new NonDismissibleHeader(mSectionHeaderView)));
        }
    }

    @VisibleForTesting
    FeedSurfaceMediator getMediatorForTesting() {
        return mMediator;
    }

    @VisibleForTesting
    View getSignInPromoViewForTesting() {
        return getSigninPromoView();
    }

    @VisibleForTesting
    View getSectionHeaderViewForTesting() {
        return getSectionHeaderView();
    }
}
