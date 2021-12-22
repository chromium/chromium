// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ScrollView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.CommandLine;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator.StreamTabId;
import org.chromium.chrome.browser.feed.sections.SectionHeaderListProperties;
import org.chromium.chrome.browser.feed.sections.SectionHeaderView;
import org.chromium.chrome.browser.feed.sections.SectionHeaderViewBinder;
import org.chromium.chrome.browser.feed.settings.FeedAutoplaySettingsFragment;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ImageCacheHelper;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides a surface that displays an interest feed rendered list of content suggestions.
 */
public class FeedSurfaceCoordinator implements FeedSurfaceProvider, FeedBubbleDelegate,
                                               SwipeRefreshLayout.OnRefreshListener,
                                               BackToTopBubbleScrollListener.ResultHandler,
                                               SurfaceCoordinator, FeedAutoplaySettingsDelegate {
    private static final String TAG = "FeedSurfaceCoordinator";

    protected final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    @Nullable
    private final View mNtpHeader;
    private final boolean mShowDarkBackground;
    private final boolean mIsPlaceholderShownInitially;
    private final FeedSurfaceDelegate mDelegate;
    private final int mDefaultMarginPixels;
    private final int mWideMarginPixels;
    private final FeedSurfaceMediator mMediator;
    private final BottomSheetController mBottomSheetController;
    private final WindowAndroid mWindowAndroid;
    private final Supplier<ShareDelegate> mShareSupplier;
    private final Handler mHandler;
    private final boolean mOverScrollDisabled;
    private final ObserverList<SurfaceCoordinator.Observer> mObservers = new ObserverList<>();
    private final FeedActionDelegate mActionDelegate;
    private final HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    private UiConfig mUiConfig;
    private FrameLayout mRootView;
    private boolean mIsActive;
    private int mHeaderCount;

    // Used when Feed is enabled.
    private @Nullable Profile mProfile;
    private @Nullable FeedSurfaceLifecycleManager mFeedSurfaceLifecycleManager;
    private @Nullable View mSigninPromoView;
    private @Nullable FeedStreamViewResizer mStreamViewResizer;
    // This is the "default"/interest feed stream, not necessarily the current stream.
    // TODO(chili): Remove the necessity of this.
    private @Nullable FeedStream mStream;
    // Feed header fields.
    private @Nullable PropertyModel mSectionHeaderModel;
    private @Nullable ViewGroup mViewportView;
    private SectionHeaderView mSectionHeaderView;
    private @Nullable ListModelChangeProcessor<PropertyListModel<PropertyModel, PropertyKey>,
            SectionHeaderView, PropertyKey> mSectionHeaderListModelChangeProcessor;
    private @Nullable PropertyModelChangeProcessor<PropertyModel, SectionHeaderView, PropertyKey>
            mSectionHeaderModelChangeProcessor;
    // Feed RecyclerView/xSurface fields.
    private @Nullable NtpListContentManager mContentManager;
    private @Nullable RecyclerView mRecyclerView;
    private @Nullable SurfaceScope mSurfaceScope;
    private @Nullable FeedSurfaceScopeDependencyProvider mDependencyProvider;
    private @Nullable HybridListRenderer mHybridListRenderer;

    // Used when Feed is disabled by policy.
    private @Nullable ScrollView mScrollViewForPolicy;
    private @Nullable ViewResizer mScrollViewResizer;

    // Used to handle things related to the main scrollable container of NTP surface.
    // In start surface, it does not track scrolling events - only the header offset.
    // In New Tab Page, it does not track the header offset (no header) - instead, it
    // tracks scrolling events.
    private @Nullable ScrollableContainerDelegate mScrollableContainerDelegate;

    private @Nullable HeaderIphScrollListener mHeaderIphScrollListener;
    private @Nullable RefreshIphScrollListener mRefreshIphScrollListener;
    private @Nullable BackToTopBubbleScrollListener mBackToTopBubbleScrollListener;

    private final FeedLaunchReliabilityLoggingState mLaunchReliabilityLoggingState;
    private FeedLaunchReliabilityLogger mLaunchReliabilityLogger;
    private final PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;

    private final Supplier<Toolbar> mToolbarSupplier;

    private FeedSwipeRefreshLayout mSwipeRefreshLayout;

    private BackToTopBubble mBackToTopBubble;

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

    private class ScrollableContainerDelegateImpl implements ScrollableContainerDelegate {
        @Override
        public void addScrollListener(ScrollListener listener) {
            if (mRecyclerView == null) return;

            mMediator.addScrollListener(listener);
        }

        @Override
        public void removeScrollListener(ScrollListener listener) {
            if (mRecyclerView == null) return;

            mMediator.removeScrollListener(listener);
        }

        @Override
        public int getVerticalScrollOffset() {
            return mMediator.getVerticalScrollOffset();
        }

        @Override
        public int getRootViewHeight() {
            return mRootView.getHeight();
        }

        @Override
        public int getTopPositionRelativeToContainerView(View childView) {
            int[] pos = new int[2];
            ViewUtils.getRelativeLayoutPosition(mRootView, childView, pos);
            return pos[1];
        }
    }

    /**
     * Constructs a new FeedSurfaceCoordinator.
     * @param activity The containing {@link Activity}.
     * @param snackbarManager The {@link SnackbarManager} displaying Snackbar UI.
     * @param windowAndroid The window of the page.
     * @param snapScrollHelper The {@link SnapScrollHelper} for the New Tab Page.
     * @param ntpHeader The extra header on top of the feeds for the New Tab Page.
     * @param toolbarHeight The height of the toolbar which overlaps Feed content at the top of the
     *   view.
     * @param showDarkBackground Whether is shown on dark background.
     * @param delegate The constructing {@link FeedSurfaceDelegate}.
     * @param profile The current user profile.
     * @param isPlaceholderShownInitially Whether the placeholder is shown initially.
     * @param bottomSheetController The bottom sheet controller.
     * @param shareDelegateSupplier The supplier for the share delegate used to share articles.
     * @param launchOrigin The origin of what launched the feed.
     * @param privacyPreferencesManager Manages the privacy preferences.
     * @param toolbarSupplier Supplies the {@link Toolbar}.
     * @param FeedLaunchReliabilityLoggingState Holds the state for feed surface creation.
     * @param swipeRefreshLayout The layout to support pull-to-refresh.
     * @param overScrollDisabled Whether the overscroll effect is disabled.
     * @param viewportView The view that should be used as a container for viewport measurement
     *   purposes, or |null| if the view returned by HybridListRenderer is to be used.
     * @param actionDelegate Implements some Feed actions.
     * @param helpAndFeedbackLauncher A HelpAndFeedbackLauncher.
     * @param feedHooks A @{link FeedHooks} instance.
     */
    public FeedSurfaceCoordinator(Activity activity, SnackbarManager snackbarManager,
            WindowAndroid windowAndroid, @Nullable SnapScrollHelper snapScrollHelper,
            @Nullable View ntpHeader, @Px int toolbarHeight, boolean showDarkBackground,
            FeedSurfaceDelegate delegate, Profile profile, boolean isPlaceholderShownInitially,
            BottomSheetController bottomSheetController,
            Supplier<ShareDelegate> shareDelegateSupplier,
            @Nullable ScrollableContainerDelegate externalScrollableContainerDelegate,
            @NewTabPageLaunchOrigin int launchOrigin,
            PrivacyPreferencesManagerImpl privacyPreferencesManager,
            @NonNull Supplier<Toolbar> toolbarSupplier,
            FeedLaunchReliabilityLoggingState launchReliabilityLoggingState,
            @Nullable FeedSwipeRefreshLayout swipeRefreshLayout, boolean overScrollDisabled,
            @Nullable ViewGroup viewportView, FeedActionDelegate actionDelegate,
            HelpAndFeedbackLauncher helpAndFeedbackLauncher) {
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mNtpHeader = ntpHeader;
        mShowDarkBackground = showDarkBackground;
        mIsPlaceholderShownInitially = isPlaceholderShownInitially;
        mDelegate = delegate;
        mBottomSheetController = bottomSheetController;
        mProfile = profile;
        mWindowAndroid = windowAndroid;
        mShareSupplier = shareDelegateSupplier;
        mScrollableContainerDelegate = externalScrollableContainerDelegate;
        mLaunchReliabilityLoggingState = launchReliabilityLoggingState;
        mPrivacyPreferencesManager = privacyPreferencesManager;
        mToolbarSupplier = toolbarSupplier;
        mSwipeRefreshLayout = swipeRefreshLayout;
        mOverScrollDisabled = overScrollDisabled;
        mViewportView = viewportView;
        mActionDelegate = actionDelegate;
        mHelpAndFeedbackLauncher = helpAndFeedbackLauncher;

        Resources resources = mActivity.getResources();
        mDefaultMarginPixels = mActivity.getResources().getDimensionPixelSize(
                R.dimen.content_suggestions_card_modern_margin);
        mWideMarginPixels = mActivity.getResources().getDimensionPixelSize(
                R.dimen.ntp_wide_card_lateral_margins);

        mRootView = new RootView(mActivity);
        mRootView.setPadding(0, resources.getDimensionPixelOffset(R.dimen.tab_strip_height), 0, 0);
        mUiConfig = new UiConfig(mRootView);

        mHandler = new Handler(Looper.getMainLooper());

        // MVC setup for feed header.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_FEED)) {
            mSectionHeaderView = (SectionHeaderView) LayoutInflater.from(mActivity).inflate(
                    R.layout.new_tab_page_multi_feed_header, null, false);
        } else {
            mSectionHeaderView = (SectionHeaderView) LayoutInflater.from(mActivity).inflate(
                    R.layout.new_tab_page_feed_v2_expandable_header, null, false);
        }
        mSectionHeaderModel = SectionHeaderListProperties.create(toolbarHeight);

        SectionHeaderViewBinder binder = new SectionHeaderViewBinder();
        mSectionHeaderModelChangeProcessor = PropertyModelChangeProcessor.create(
                mSectionHeaderModel, mSectionHeaderView, binder);
        mSectionHeaderListModelChangeProcessor = new ListModelChangeProcessor<>(
                mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY),
                mSectionHeaderView, binder);
        mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                .addObserver(mSectionHeaderListModelChangeProcessor);

        // Mediator should be created before any Stream changes.
        mMediator = new FeedSurfaceMediator(this, mActivity, snapScrollHelper, mSectionHeaderModel,
                getTabIdFromLaunchOrigin(launchOrigin), actionDelegate);

        FeedSurfaceTracker.getInstance().trackSurface(this);

        // Creates streams, initiates content changes.
        mMediator.updateContent();

        // Enable pull-to-refresh.
        if (mSwipeRefreshLayout != null) {
            mSwipeRefreshLayout.enableSwipe(externalScrollableContainerDelegate);
            mSwipeRefreshLayout.addOnRefreshListener(this);
        }
    }

    private void stopScrollTracking() {
        if (mScrollableContainerDelegate != null) {
            mScrollableContainerDelegate.removeScrollListener(mDependencyProvider);
            mScrollableContainerDelegate = null;
        }
    }

    @Override
    public void destroy() {
        if (mSwipeRefreshLayout != null) {
            if (mSwipeRefreshLayout.isRefreshing()) {
                mSwipeRefreshLayout.setRefreshing(false);
                updateReloadButtonVisibility(/*isReloading=*/false);
            }
            mSwipeRefreshLayout.removeOnRefreshListener(this);
            mSwipeRefreshLayout.disableSwipe();
            mSwipeRefreshLayout = null;
        }
        stopBubbleTriggering();
        if (mFeedSurfaceLifecycleManager != null) mFeedSurfaceLifecycleManager.destroy();
        mFeedSurfaceLifecycleManager = null;
        stopScrollTracking();
        if (mSectionHeaderModelChangeProcessor != null) {
            mSectionHeaderModelChangeProcessor.destroy();
            mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                    .removeObserver(mSectionHeaderListModelChangeProcessor);
        }
        // Destroy mediator after all other related controller/processors are destroyed.
        mMediator.destroy();

        maybeClearImageMemoryCache();
        FeedSurfaceTracker.getInstance().untrackSurface(this);
        if (mHybridListRenderer != null) {
            mHybridListRenderer.unbind();
        }
        mRootView.removeAllViews();
    }

    /**
     * Enables/disables the pull-to-refresh.
     *
     * @param enabled Whether the pull-to-refresh should be enabled.
     */
    public void enableSwipeRefresh(boolean enabled) {
        if (mSwipeRefreshLayout != null) {
            if (enabled) {
                mSwipeRefreshLayout.enableSwipe(null);
            } else {
                mSwipeRefreshLayout.disableSwipe();
            }
        }
    }

    @Override
    public TouchEnabledDelegate getTouchEnabledDelegate() {
        return mMediator;
    }

    @Override
    public FeedSurfaceScrollDelegate getScrollDelegate() {
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

    @Override
    public void onRefresh() {
        updateReloadButtonVisibility(/*isReloading=*/true);
        // It is possible that onRefresh() is called when mLaunchReliabilityLogger is null, see
        // https://crbug.com/1269056.
        if (mLaunchReliabilityLogger != null) {
            mLaunchReliabilityLogger.logManualRefresh(System.nanoTime());
        }
        mMediator.manualRefresh((Boolean v) -> {
            if (mSwipeRefreshLayout == null) return;
            updateReloadButtonVisibility(/*isReloading=*/false);
            mSwipeRefreshLayout.setRefreshing(false);
        });
        getFeatureEngagementTracker().notifyEvent(EventConstants.FEED_SWIPE_REFRESHED);
    }

    void updateReloadButtonVisibility(boolean isReloading) {
        Toolbar toolbar = mToolbarSupplier.get();
        if (toolbar != null) {
            toolbar.updateReloadButtonVisibility(isReloading);
        }
    }

    /**
     * @return The {@link FeedSurfaceLifecycleManager} that manages the lifecycle of the {@link
     *         Stream}.
     */
    FeedSurfaceLifecycleManager getSurfaceLifecycleManager() {
        return mFeedSurfaceLifecycleManager;
    }

    /** @return The {@link Stream} that this class holds. */
    Stream getStream() {
        return mStream;
    }

    /** @return Whether the placeholder is shown. */
    public boolean isPlaceholderShown() {
        return mStream != null ? mStream.isPlaceholderShown() : false;
    }

    /** Launches autoplay settings activity. */
    @Override
    public void launchAutoplaySettings() {
        SettingsLauncher launcher = new SettingsLauncherImpl();
        launcher.launchSettingsActivity(
                mActivity, FeedAutoplaySettingsFragment.class, new Bundle());
    }

    /** @return whether this coordinator is currently active. */
    @Override
    public boolean isActive() {
        return mIsActive;
    }

    /** Shows the feed. */
    @Override
    public void onSurfaceOpened() {
        // Guard on isStartupCalled.
        if (!FeedSurfaceTracker.getInstance().isStartupCalled()) return;
        mIsActive = true;
        for (Observer observer : mObservers) {
            observer.surfaceOpened();
        }
        mMediator.onSurfaceOpened();
    }

    /** Hides the feed. */
    @Override
    public void onSurfaceClosed() {
        if (!FeedSurfaceTracker.getInstance().isStartupCalled()) return;
        mIsActive = false;
        mMediator.onSurfaceClosed();
    }

    /** Returns a string usable for restoring the UI to current state. */
    @Override
    public String getSavedInstanceStateString() {
        return mMediator.getSavedInstanceString();
    }

    /** Restores the UI to a previously saved state. */
    @Override
    public void restoreInstanceState(String state) {
        mMediator.restoreSavedInstanceState(state);
    }

    /** Sets the {@link StreamTabId} of the feed given a {@link NewTabPageLaunchOrigin}. */
    public void setTabIdFromLaunchOrigin(@NewTabPageLaunchOrigin int launchOrigin) {
        mMediator.setTabId(getTabIdFromLaunchOrigin(launchOrigin));
    }

    /**
     * Gets the appropriate {@link StreamTabId} for the given {@link NewTabPageLaunchOrigin}.
     *
     * <p>If coming from a Web Feed button, open the following tab, otherwise open the for you tab.
     */
    @VisibleForTesting
    @StreamTabId
    int getTabIdFromLaunchOrigin(@NewTabPageLaunchOrigin int launchOrigin) {
        return launchOrigin == NewTabPageLaunchOrigin.WEB_FEED ? StreamTabId.FOLLOWING
                                                               : StreamTabId.DEFAULT;
    }

    private RecyclerView setUpView() {
        mContentManager = new NtpListContentManager();
        Context context = new ContextThemeWrapper(mActivity,
                (mShowDarkBackground ? R.style.ThemeOverlay_Feed_Dark
                                     : R.style.ThemeOverlay_Feed_Light));
        ProcessScope processScope = FeedSurfaceTracker.getInstance().getXSurfaceProcessScope();
        if (processScope != null) {
            mDependencyProvider = new FeedSurfaceScopeDependencyProvider(
                    mActivity, context, mShowDarkBackground, () -> {
                        if (mMediator.getFirstStream() == null) return false;
                        return mMediator.getFirstStream().isActivityLoggingEnabled();
                    });

            mSurfaceScope = processScope.obtainSurfaceScope(mDependencyProvider);
            if (mScrollableContainerDelegate != null) {
                mScrollableContainerDelegate.addScrollListener(mDependencyProvider);
            }
        } else {
            mDependencyProvider = null;
            mSurfaceScope = null;
        }

        if (mSurfaceScope != null) {
            mHybridListRenderer = mSurfaceScope.provideListRenderer();

            if (isReliabilityLoggingEnabled()) {
                mLaunchReliabilityLogger = mSurfaceScope.getFeedLaunchReliabilityLogger();
                mLaunchReliabilityLoggingState.onLoggerAvailable(mLaunchReliabilityLogger);
            }

        } else {
            mHybridListRenderer = new NativeViewListRenderer(context);
        }

        if (mLaunchReliabilityLogger == null) {
            // No-op logger.
            mLaunchReliabilityLogger = new FeedLaunchReliabilityLogger() {};
        }

        RecyclerView view;
        if (mHybridListRenderer != null) {
            // XSurface returns a View, but it should be a RecyclerView.
            view = (RecyclerView) mHybridListRenderer.bind(mContentManager, mViewportView);
            view.setId(R.id.feed_stream_recycler_view);
            view.setClipToPadding(false);
            view.setBackgroundColor(
                    MaterialColors.getColor(context, R.attr.default_bg_color_dynamic, TAG));
        } else {
            view = null;
        }
        return view;
    }

    /** @return The {@link RecyclerView} associated with this feed. */
    public RecyclerView getRecyclerView() {
        return mRecyclerView;
    }

    /** @return The {@link SurfaceScope} used to create this feed. */
    SurfaceScope getSurfaceScope() {
        return mSurfaceScope;
    }

    /** @return The {@link HybridListRenderer} used to render this feed. */
    HybridListRenderer getHybridListRenderer() {
        return mHybridListRenderer;
    }

    /** @return The {@link NtpListContentManager} managing the contents of this feed. */
    NtpListContentManager getContentManager() {
        return mContentManager;
    }

    /** @return Returns this surface's {@link FeedLaunchReliabilityLogger}. */
    public FeedLaunchReliabilityLogger getLaunchReliabilityLogger() {
        return mLaunchReliabilityLogger;
    }

    /**
     * Create a {@link Stream} for this class.
     */
    void createStream() {
        assert mStream == null;

        if (mScrollViewForPolicy != null) {
            mRootView.removeView(mScrollViewForPolicy);
            mScrollViewForPolicy = null;
            mScrollViewResizer.detach();
            mScrollViewResizer = null;
        }
        mRecyclerView = setUpView();

        mStream = createFeedStream(true);
        mActionDelegate.onStreamCreated();
        mFeedSurfaceLifecycleManager = mDelegate.createStreamLifecycleManager(mActivity, this);
        mRecyclerView.setBackgroundColor(
                MaterialColors.getColor(mActivity, R.attr.default_bg_color_dynamic, TAG));

        // For New Tab Page, mSwipeRefreshLayout has not been added to a view container. We need to
        // do it here.
        if (mSwipeRefreshLayout != null && mSwipeRefreshLayout.getParent() == null) {
            mRootView.addView(mSwipeRefreshLayout);
            mSwipeRefreshLayout.addView(mRecyclerView);
        } else {
            mRootView.addView(mRecyclerView);
        }

        mStreamViewResizer = FeedStreamViewResizer.createAndAttach(
                mActivity, mRecyclerView, mUiConfig, mDefaultMarginPixels, mWideMarginPixels);

        if (mNtpHeader != null) UiUtils.removeViewFromParent(mNtpHeader);
        UiUtils.removeViewFromParent(mSectionHeaderView);
        if (mSigninPromoView != null) UiUtils.removeViewFromParent(mSigninPromoView);

        // Directly add header views to content manager.
        List<View> headerList = new ArrayList<>();
        if (mNtpHeader != null) {
            headerList.add(mNtpHeader);
        }
        headerList.add(mSectionHeaderView);

        setHeaders(headerList);

        // Work around https://crbug.com/943873 where default focus highlight shows up after
        // toggling dark mode.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mRecyclerView.setDefaultFocusHighlightEnabled(false);
        }

        // Explicitly request focus on the scroll container to avoid UrlBar being focused after
        // the scroll container for policy is removed.
        mRecyclerView.requestFocus();

        if (mOverScrollDisabled) {
            mRecyclerView.setOverScrollMode(View.OVER_SCROLL_NEVER);
        }
    }

    /**
     * Creates a flavor {@Link FeedStream} without any other side-effects.
     *
     * @param isInterestFeed True for interest feed, false for web feed.
     * @return The FeedStream created.
     */
    FeedStream createFeedStream(boolean isInterestFeed) {
        return new FeedStream(mActivity, mSnackbarManager, mBottomSheetController,
                mIsPlaceholderShownInitially, mWindowAndroid, mShareSupplier, isInterestFeed, this,
                mActionDelegate, mHelpAndFeedbackLauncher);
    }

    private void setHeaders(List<View> headerViews) {
        // Build the list of headers we want, and then replace existing headers.
        List<NtpListContentManager.FeedContent> headerList = new ArrayList<>();
        for (View header : headerViews) {
            // Feed header view in multi does not need padding added.
            int lateralPaddingsPx = getLateralPaddingsPx();
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_FEED)
                    && header == mSectionHeaderView) {
                lateralPaddingsPx = 0;
            }

            NtpListContentManager.NativeViewContent content =
                    new NtpListContentManager.NativeViewContent(
                            lateralPaddingsPx, "Header" + header.hashCode(), header);
            headerList.add(content);
        }
        if (mContentManager.replaceRange(0, mHeaderCount, headerList)) {
            mHeaderCount = headerList.size();
            mMediator.notifyHeadersChanged(mHeaderCount);
        }
    }

    /**
     * @return The {@link ScrollView} for displaying content for supervised user or enterprise
     *         policy.
     */
    @VisibleForTesting
    public ScrollView getScrollViewForPolicy() {
        return mScrollViewForPolicy;
    }

    /**
     * Create a {@link ScrollView} for displaying content for supervised user or enterprise policy.
     */
    void createScrollViewForPolicy() {
        if (mStream != null) {
            mStreamViewResizer.detach();
            mStreamViewResizer = null;
            mRootView.removeView(mRecyclerView);
            assert mFeedSurfaceLifecycleManager
                    != null
                : "SurfaceLifecycleManager should not be null when the Stream is not null.";
            mFeedSurfaceLifecycleManager.destroy();
            mFeedSurfaceLifecycleManager = null;
            mStream = null;
            mSigninPromoView = null;
        }

        mScrollViewForPolicy = new PolicyScrollView(mActivity);
        mScrollViewForPolicy.setBackgroundColor(
                MaterialColors.getColor(mActivity, R.attr.default_bg_color_dynamic, TAG));
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
        mHeaderCount = 0;

        mRootView.addView(mScrollViewForPolicy);
        mScrollViewResizer = ViewResizer.createAndAttach(
                mScrollViewForPolicy, mUiConfig, mDefaultMarginPixels, mWideMarginPixels);
        mScrollViewForPolicy.requestFocus();
    }

    /** @return The {@link SectionHeaderListProperties} model for the Feed section header. */
    @VisibleForTesting
    PropertyModel getSectionHeaderModelForTest() {
        return mSectionHeaderModel;
    }

    /** @return The {@link View} for this class. */
    View getSigninPromoView() {
        if (mSigninPromoView == null) {
            LayoutInflater inflater = LayoutInflater.from(mRootView.getContext());
            mSigninPromoView = inflater.inflate(
                    R.layout.personalized_signin_promo_view_modern_content_suggestions, mRootView,
                    false);
        }
        return mSigninPromoView;
    }

    /**
     * Update header views in the Feed.
     */
    void updateHeaderViews(boolean isSignInPromoVisible) {
        if (mStream == null) return;

        List<View> headers = new ArrayList<>();
        if (mNtpHeader != null) {
            assert mSectionHeaderView != null;
            headers.add(mNtpHeader);
        }

        headers.add(mSectionHeaderView);

        if (isSignInPromoVisible) {
            headers.add(getSigninPromoView());
        }
        setHeaders(headers);
    }

    @VisibleForTesting
    public FeedSurfaceMediator getMediatorForTesting() {
        return mMediator;
    }

    @VisibleForTesting
    public View getSignInPromoViewForTesting() {
        return getSigninPromoView();
    }

    @VisibleForTesting
    public View getSectionHeaderViewForTesting() {
        return mSectionHeaderView;
    }

    @VisibleForTesting
    public BackToTopBubble getBackToTopBubble() {
        return mBackToTopBubble;
    }

    /**
     * Initializes things related to the bubbles which will start listening to scroll events to
     * determine whether a bubble should be triggered.
     *
     * You must stop the IPH with #stopBubbleTriggering before tearing down feed components, e.g.,
     * on #destroy. This also applies for the case where the feed stream is deleted when disabled
     * (e.g., by policy).
     */
    void initializeBubbleTriggering() {
        // Don't do anything when there is no feed stream because the bubble isn't needed in that
        // case.
        if (mStream == null) return;

        // Provide a delegate for the container of the feed surface that is handled by the feed
        // coordinator itself when not provided externally (e.g., by the NewTabPage).
        if (mScrollableContainerDelegate == null) {
            mScrollableContainerDelegate = new ScrollableContainerDelegateImpl();
        }

        createHeaderIphScrollListener();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_INTERACTIVE_REFRESH)) {
            createRefreshIphScrollListener();
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_BACK_TO_TOP)) {
            createBackToTopBubbleScrollListener();
        }
    }

    private void createHeaderIphScrollListener() {
        mHeaderIphScrollListener =
                new HeaderIphScrollListener(this, mScrollableContainerDelegate, () -> {
                    UserEducationHelper helper = new UserEducationHelper(mActivity, mHandler);
                    mSectionHeaderView.showMenuIph(helper);
                });
        mScrollableContainerDelegate.addScrollListener(mHeaderIphScrollListener);
    }

    private void createRefreshIphScrollListener() {
        mRefreshIphScrollListener =
                new RefreshIphScrollListener(this, mScrollableContainerDelegate, () -> {
                    UserEducationHelper helper = new UserEducationHelper(mActivity, mHandler);
                    mSwipeRefreshLayout.showIPH(helper);
                });
        mScrollableContainerDelegate.addScrollListener(mRefreshIphScrollListener);
    }

    private void createBackToTopBubbleScrollListener() {
        mBackToTopBubbleScrollListener = new BackToTopBubbleScrollListener(this, this);
        mScrollableContainerDelegate.addScrollListener(mBackToTopBubbleScrollListener);
    }

    /**
     * Stops and deletes things related to the bubbles. Must be called before tearing down feed
     * components, e.g., on #destroy. This also applies for the case where the feed stream is
     * deleted when disabled (e.g., by policy).
     */
    private void stopBubbleTriggering() {
        if (mStream != null && mScrollableContainerDelegate != null) {
            if (mHeaderIphScrollListener != null) {
                mScrollableContainerDelegate.removeScrollListener(mHeaderIphScrollListener);
                mHeaderIphScrollListener = null;
            }
            if (mRefreshIphScrollListener != null) {
                mScrollableContainerDelegate.removeScrollListener(mRefreshIphScrollListener);
                mRefreshIphScrollListener = null;
            }
            if (mBackToTopBubbleScrollListener != null) {
                mScrollableContainerDelegate.removeScrollListener(mBackToTopBubbleScrollListener);
                mBackToTopBubbleScrollListener = null;
            }
        }
        stopScrollTracking();
    }

    @Override
    public Tracker getFeatureEngagementTracker() {
        return TrackerFactory.getTrackerForProfile(mProfile);
    }

    @Override
    public boolean isFeedExpanded() {
        return mSectionHeaderModel.get(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY);
    }

    @Override
    public boolean isSignedIn() {
        return IdentityServicesProvider.get()
                .getSigninManager(Profile.getLastUsedRegularProfile())
                .getIdentityManager()
                .hasPrimaryAccount(ConsentLevel.SYNC);
    }

    @Override
    public boolean isFeedHeaderPositionInContainerSuitableForIPH(float headerMaxPosFraction) {
        assert headerMaxPosFraction >= 0.0f
                && headerMaxPosFraction <= 1.0f
            : "Max position fraction should be ranging between 0.0 and 1.0";

        int topPosInStream = mScrollableContainerDelegate.getTopPositionRelativeToContainerView(
                mSectionHeaderView);
        if (topPosInStream < 0) return false;
        if (topPosInStream
                > headerMaxPosFraction * mScrollableContainerDelegate.getRootViewHeight()) {
            return false;
        }

        return true;
    }

    @Override
    public long getCurrentTimeMs() {
        return System.currentTimeMillis();
    }

    @Override
    public long getLastFetchTimeMs() {
        return (mStream == null) ? 0 : mStream.getLastFetchTimeMs();
    }

    @Override
    public boolean canScrollUp() {
        // mSwipeRefreshLayout is set to NULL when this instance is destroyed, but
        // RefreshIphScrollListener.onHeaderOffsetChanged may still be triggered which will call
        // into this method.
        return (mSwipeRefreshLayout == null) ? true : mSwipeRefreshLayout.canScrollVertically(-1);
    }

    @Override
    public boolean isShowingBackToTopBubble() {
        return mBackToTopBubble != null && mBackToTopBubble.isShowing();
    }

    @Override
    public int getHeaderCount() {
        return mHeaderCount;
    }

    @Override
    public int getItemCount() {
        return ((LinearLayoutManager) mRecyclerView.getLayoutManager()).getItemCount();
    }

    @Override
    public int getFirstVisiblePosition() {
        return ((LinearLayoutManager) mRecyclerView.getLayoutManager())
                .findFirstVisibleItemPosition();
    }

    @Override
    public int getLastVisiblePosition() {
        return ((LinearLayoutManager) mRecyclerView.getLayoutManager())
                .findLastVisibleItemPosition();
    }

    @Override
    public void showBubble() {
        if (mBackToTopBubble != null) return;
        mBackToTopBubble = new BackToTopBubble(mActivity, mRootView.getContext(), mRootView, () -> {
            mBackToTopBubble.dismiss();
            mBackToTopBubble = null;
            mRecyclerView.smoothScrollToPosition(0);
        });
        mBackToTopBubble.show();
    }

    @Override
    public void dismissBubble() {
        if (mBackToTopBubble == null) return;
        mBackToTopBubble.dismiss();
        mBackToTopBubble = null;
    }

    @Override
    public void addObserver(SurfaceCoordinator.Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(SurfaceCoordinator.Observer observer) {
        mObservers.removeObserver(observer);
    }

    public boolean isLoadingFeed() {
        return mMediator.isLoadingFeed();
    }

    private boolean isReliabilityLoggingEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_RELIABILITY_LOGGING)
                && (mPrivacyPreferencesManager.isMetricsReportingEnabled()
                        || CommandLine.getInstance().hasSwitch(
                                "force-enable-feed-reliability-logging"));
    }

    // Clear the memory cache if the FEED_CLEAR_IMAGE_MEMORY_CACHE flag is enabled.
    private void maybeClearImageMemoryCache() {
        ProcessScope processScope = FeedSurfaceTracker.getInstance().getXSurfaceProcessScope();
        if (processScope != null) {
            ImageCacheHelper imageCacheHelper = processScope.provideImageCacheHelper();
            if (imageCacheHelper != null
                    && ChromeFeatureList.isEnabled(
                            ChromeFeatureList.FEED_CLEAR_IMAGE_MEMORY_CACHE)) {
                imageCacheHelper.clearMemoryCache();
            }
        }
    }

    private int getLateralPaddingsPx() {
        return mActivity.getResources().getDimensionPixelSize(
                R.dimen.ntp_header_lateral_paddings_v2);
    }
}
