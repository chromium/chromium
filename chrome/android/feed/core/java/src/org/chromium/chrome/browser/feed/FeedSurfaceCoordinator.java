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
import android.os.SystemClock;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ScrollView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.settings.FeedAutoplaySettingsFragment;
import org.chromium.chrome.browser.feed.shared.FeedSurfaceDelegate;
import org.chromium.chrome.browser.feed.shared.FeedSurfaceProvider;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.feed.v2.FeedStream;
import org.chromium.chrome.browser.feed.v2.NativeViewListRenderer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.ScrollListener;
import org.chromium.chrome.browser.ntp.ScrollableContainerDelegate;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.cards.promo.enhanced_protection.EnhancedProtectionPromoController;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderListProperties;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderView;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderViewBinder;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;
import org.chromium.components.feature_engagement.Tracker;
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
public class FeedSurfaceCoordinator
        implements FeedSurfaceProvider, FeedIPHDelegate, SwipeRefreshLayout.OnRefreshListener {
    @VisibleForTesting
    public static final String FEED_STREAM_CREATED_TIME_MS_UMA = "FeedStreamCreatedTime";

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

    private UiConfig mUiConfig;
    private FrameLayout mRootView;
    private long mStreamCreatedTimeMs;
    private boolean mIsActive;
    private int mHeaderCount;

    // Enhanced Protection promo view will be not-null once we have it created, until it is
    // destroyed.
    private @Nullable View mEnhancedProtectionPromoView;
    private @Nullable EnhancedProtectionPromoController mEnhancedProtectionPromoController;

    // Used when Feed is enabled.
    private @Nullable Profile mProfile;
    private @Nullable NativePageNavigationDelegate mPageNavigationDelegate;
    private @Nullable FeedSurfaceLifecycleManager mFeedSurfaceLifecycleManager;
    private @Nullable View mSigninPromoView;
    private @Nullable FeedStreamViewResizer mStreamViewResizer;
    // This is the "default"/interest feed stream, not necessarily the current stream.
    // TODO(chili): Remove the necessity of this.
    private @Nullable FeedStream mStream;
    // Feed header fields.
    private @Nullable PropertyModel mSectionHeaderModel;
    private @Nullable SectionHeaderView mSectionHeaderView;
    private @Nullable ListModelChangeProcessor<PropertyListModel<PropertyModel, PropertyKey>,
            SectionHeaderView, PropertyKey> mSectionHeaderListModelChangeProcessor;
    private @Nullable PropertyModelChangeProcessor<PropertyModel, SectionHeaderView, PropertyKey>
            mSectionHeaderModelChangeProcessor;
    // Feed RecyclerView/xSurface fields.
    private @Nullable NtpListContentManager mContentManager;
    private @Nullable RecyclerView mRecyclerView;
    private @Nullable SurfaceScope mSurfaceScope;
    private @Nullable HybridListRenderer mHybridListRenderer;

    // Used when Feed is disabled by policy.
    private @Nullable ScrollView mScrollViewForPolicy;
    private @Nullable ViewResizer mScrollViewResizer;

    // Used to handle things related to the main scrollable container of NTP surface.
    private @Nullable ScrollableContainerDelegate mScrollableContainerDelegate;

    private @Nullable HeaderIphScrollListener mHeaderIphScrollListener;
    private @Nullable RefreshIphScrollListener mRefreshIphScrollListener;

    private final FeedLaunchReliabilityLoggingState mLaunchReliabilityLoggingState;
    private FeedLaunchReliabilityLogger mLaunchReliabilityLogger;
    private final PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;

    private FeedSwipeRefreshLayout mSwipeRefreshLayout;

    @IntDef({StreamTabId.FOR_YOU, StreamTabId.FOLLOWING})
    public @interface StreamTabId {
        int FOR_YOU = 0;
        int FOLLOWING = 1;
    };

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
     * @param sectionHeaderView The {@link SectionHeaderView} for the feed.
     * @param showDarkBackground Whether is shown on dark background.
     * @param delegate The constructing {@link FeedSurfaceDelegate}.
     * @param pageNavigationDelegate The {@link NativePageNavigationDelegate}
     *                               that handles page navigation.
     * @param profile The current user profile.
     * @param isPlaceholderShownInitially Whether the placeholder is shown initially.
     * @param bottomSheetController The bottom sheet controller.
     * @param shareDelegateSupplier The supplier for the share delegate used to share articles.
     * @param launchOrigin The origin of what launched the feed.
     */
    public FeedSurfaceCoordinator(Activity activity, SnackbarManager snackbarManager,
            WindowAndroid windowAndroid, @Nullable SnapScrollHelper snapScrollHelper,
            @Nullable View ntpHeader, @Nullable SectionHeaderView sectionHeaderView,
            boolean showDarkBackground, FeedSurfaceDelegate delegate,
            @Nullable NativePageNavigationDelegate pageNavigationDelegate, Profile profile,
            boolean isPlaceholderShownInitially, BottomSheetController bottomSheetController,
            Supplier<ShareDelegate> shareDelegateSupplier,
            @Nullable ScrollableContainerDelegate externalScrollableContainerDelegate,
            @NewTabPageLaunchOrigin int launchOrigin,
            PrivacyPreferencesManagerImpl privacyPreferencesManager,
            FeedLaunchReliabilityLoggingState launchReliabilityLoggingState,
            @Nullable FeedSwipeRefreshLayout swipeRefreshLayout) {
        FeedSurfaceTracker.getInstance().initServiceBridge();
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mNtpHeader = ntpHeader;
        mShowDarkBackground = showDarkBackground;
        mIsPlaceholderShownInitially = isPlaceholderShownInitially;
        mDelegate = delegate;
        mPageNavigationDelegate = pageNavigationDelegate;
        mBottomSheetController = bottomSheetController;
        mProfile = profile;
        mWindowAndroid = windowAndroid;
        mShareSupplier = shareDelegateSupplier;
        mScrollableContainerDelegate = externalScrollableContainerDelegate;
        mLaunchReliabilityLoggingState = launchReliabilityLoggingState;
        mPrivacyPreferencesManager = privacyPreferencesManager;
        mSwipeRefreshLayout = swipeRefreshLayout;

        Resources resources = mActivity.getResources();
        mDefaultMarginPixels = mActivity.getResources().getDimensionPixelSize(
                R.dimen.content_suggestions_card_modern_margin);
        mWideMarginPixels = mActivity.getResources().getDimensionPixelSize(
                R.dimen.ntp_wide_card_lateral_margins);

        mRootView = new RootView(mActivity);
        mRootView.setPadding(0, resources.getDimensionPixelOffset(R.dimen.tab_strip_height), 0, 0);
        mUiConfig = new UiConfig(mRootView);

        mHandler = new Handler(Looper.getMainLooper());

        if (isEnhancedProtectionPromoEnabled()) {
            mEnhancedProtectionPromoController =
                    new EnhancedProtectionPromoController(mActivity, mProfile);
        }

        // MVC setup for feed header.
        mSectionHeaderView = sectionHeaderView;
        mSectionHeaderModel = SectionHeaderListProperties.create();
        if (mSectionHeaderView != null) {
            SectionHeaderViewBinder binder = new SectionHeaderViewBinder();
            mSectionHeaderModelChangeProcessor = PropertyModelChangeProcessor.create(
                    mSectionHeaderModel, mSectionHeaderView, binder);
            mSectionHeaderListModelChangeProcessor = new ListModelChangeProcessor<>(
                    mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY),
                    mSectionHeaderView, binder);
            mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                    .addObserver(mSectionHeaderListModelChangeProcessor);
        }

        // Mediator should be created before any Stream changes.
        mMediator =
                new FeedSurfaceMediator(this, mActivity, snapScrollHelper, mPageNavigationDelegate,
                        mSectionHeaderModel, getTabIdFromLaunchOrigin(launchOrigin));

        // Creates streams, initiates content changes.
        mMediator.updateContent();
        FeedSurfaceTracker.getInstance().trackSurface(this);

        // Enable pull-to-refresh.
        if (mSwipeRefreshLayout != null) {
            mSwipeRefreshLayout.enableSwipe(externalScrollableContainerDelegate);
            mSwipeRefreshLayout.addOnRefreshListener(this);
        }
    }

    @Override
    public void destroy() {
        if (mSwipeRefreshLayout != null) {
            mSwipeRefreshLayout.removeOnRefreshListener(this);
            mSwipeRefreshLayout.disableSwipe();
        }
        stopIph();
        mMediator.destroy();
        if (mFeedSurfaceLifecycleManager != null) mFeedSurfaceLifecycleManager.destroy();
        mFeedSurfaceLifecycleManager = null;
        if (mEnhancedProtectionPromoController != null) {
            mEnhancedProtectionPromoController.destroy();
        }
        mScrollableContainerDelegate = null;
        if (mSectionHeaderModelChangeProcessor != null) {
            mSectionHeaderModelChangeProcessor.destroy();
            mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                    .removeObserver(mSectionHeaderListModelChangeProcessor);
        }
        FeedSurfaceTracker.getInstance().untrackSurface(this);
        if (mHybridListRenderer != null) {
            mHybridListRenderer.unbind();
        }
        mRootView.removeAllViews();
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

    @Override
    public void onRefresh() {
        mStream.triggerRefresh((Boolean v) -> { mSwipeRefreshLayout.setRefreshing(false); });
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
    public void launchAutoplaySettings() {
        SettingsLauncher launcher = new SettingsLauncherImpl();
        launcher.launchSettingsActivity(
                mActivity, FeedAutoplaySettingsFragment.class, new Bundle());
    }

    /** @return whether this coordinator is currently active. */
    boolean isActive() {
        return mIsActive;
    }

    /** Shows the feed. */
    public void onSurfaceOpened() {
        // Guard on isStartupCalled.
        if (!FeedSurfaceTracker.getInstance().isStartupCalled()) return;
        mIsActive = true;

        mMediator.onSurfaceOpened();
    }

    /** Hides the feed. */
    public void onSurfaceClosed() {
        if (!FeedSurfaceTracker.getInstance().isStartupCalled()) return;
        mIsActive = false;
        mMediator.onSurfaceClosed();
    }

    /** Returns a string usable for restoring the UI to current state. */
    public String getSavedInstanceStateString() {
        return mMediator.getSavedInstanceString();
    }

    /** Restores the UI to a previously saved state. */
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
                                                               : StreamTabId.FOR_YOU;
    }

    private RecyclerView setUpView() {
        mContentManager = new NtpListContentManager();
        Context context = new ContextThemeWrapper(mActivity,
                (mShowDarkBackground ? R.style.ThemeOverlay_Feed_Dark
                                     : R.style.ThemeOverlay_Feed_Light));
        ProcessScope processScope = FeedSurfaceTracker.getInstance().getXSurfaceProcessScope();
        if (processScope != null) {
            mSurfaceScope = processScope.obtainSurfaceScope(new FeedSurfaceScopeDependencyProvider(
                    mActivity, context, mShowDarkBackground, () -> {
                        if (mMediator.getFirstStream() == null) return false;
                        return mMediator.getFirstStream().isActivityLoggingEnabled();
                    }));
        } else {
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
            view = (RecyclerView) mHybridListRenderer.bind(mContentManager);
            view.setId(R.id.feed_stream_recycler_view);
            view.setClipToPadding(false);
            view.setBackgroundColor(mActivity.getResources().getColor(R.color.default_bg_color));
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

        mStreamCreatedTimeMs = SystemClock.elapsedRealtime();
        mStream = createFeedStream(true);
        mFeedSurfaceLifecycleManager = mDelegate.createStreamLifecycleManager(mActivity, this);
        mRecyclerView.setBackgroundResource(R.color.default_bg_color);

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
        if (mSectionHeaderView != null) UiUtils.removeViewFromParent(mSectionHeaderView);
        if (mSigninPromoView != null) UiUtils.removeViewFromParent(mSigninPromoView);
        if (mEnhancedProtectionPromoView != null) {
            UiUtils.removeViewFromParent(mEnhancedProtectionPromoView);
        }

        // Directly add header views to content manager.
        List<View> headerList = new ArrayList<>();
        if (mNtpHeader != null) {
            headerList.add(mNtpHeader);
        }
        if (mSectionHeaderView != null) {
            headerList.add(mSectionHeaderView);
        }
        setHeaders(headerList);

        // Work around https://crbug.com/943873 where default focus highlight shows up after
        // toggling dark mode.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mRecyclerView.setDefaultFocusHighlightEnabled(false);
        }

        // Explicitly request focus on the scroll container to avoid UrlBar being focused after
        // the scroll container for policy is removed.
        mRecyclerView.requestFocus();
    }

    /**
     * Creates a flavor {@Link FeedStream} without any other side-effects.
     *
     * @param isInterestFeed True for interest feed, false for web feed.
     * @return The FeedStream created.
     */
    FeedStream createFeedStream(boolean isInterestFeed) {
        return new FeedStream(mActivity, mSnackbarManager, mPageNavigationDelegate,
                mBottomSheetController, mIsPlaceholderShownInitially, mWindowAndroid,
                mShareSupplier, isInterestFeed);
    }

    private void setHeaders(List<View> headerViews) {
        // Remove current headers.
        if (mHeaderCount > 0) {
            mContentManager.removeContents(0, mHeaderCount);
        }

        // Add new headers.
        List<NtpListContentManager.FeedContent> headerList = new ArrayList<>();
        for (View header : headerViews) {
            headerList.add(new NtpListContentManager.NativeViewContent(
                    "Header" + header.hashCode(), header));
        }
        mHeaderCount = headerList.size();
        if (mHeaderCount > 0) {
            mContentManager.addContents(0, headerList);
        }
        mMediator.notifyHeadersChanged(mHeaderCount);
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

            mEnhancedProtectionPromoView = null;
            if (mEnhancedProtectionPromoController != null) {
                mEnhancedProtectionPromoController.destroy();
                mEnhancedProtectionPromoController = null;
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
    void updateHeaderViews(
            boolean isSignInPromoVisible, @Nullable View enhancedProtectionPromoView) {
        if (mStream == null) return;

        List<View> headers = new ArrayList<>();
        if (mNtpHeader != null) {
            assert mSectionHeaderView != null;
            headers.add(mNtpHeader);
        }

        if (enhancedProtectionPromoView != null) {
            mEnhancedProtectionPromoView = enhancedProtectionPromoView;
            headers.add(enhancedProtectionPromoView);
        }

        if (mSectionHeaderView != null) {
            headers.add(mSectionHeaderView);
        }

        if (isSignInPromoVisible) {
            headers.add(getSigninPromoView());
        }
        setHeaders(headers);
    }

    public void onOverviewShownAtLaunch(long activityCreationTimeMs) {
        mMediator.onOverviewShownAtLaunch(activityCreationTimeMs, mIsPlaceholderShownInitially);
        StartSurfaceConfiguration.recordHistogram(FEED_STREAM_CREATED_TIME_MS_UMA,
                mStreamCreatedTimeMs - activityCreationTimeMs, mIsPlaceholderShownInitially);
    }

    EnhancedProtectionPromoController getEnhancedProtectionPromoController() {
        return mEnhancedProtectionPromoController;
    }

    private boolean isEnhancedProtectionPromoEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD);
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

    /**
     * Initializes things related to the IPH which will start listening to scroll events to
     * determine whether the IPH should be triggered.
     *
     * You must stop the IPH with #stopIph before tearing down feed components, e.g., on #destroy.
     * This also applies for the case where the feed stream is deleted when disabled (e.g., by
     * policy).
     */
    void initializeIph() {
        // Don't do anything when there is no feed stream because the IPH isn't needed in that
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

    /**
     * Stops and deletes things related to the IPH. Must be called before tearing down feed
     * components, e.g., on #destroy. This also applies for the case where the feed stream is
     * deleted when disabled (e.g., by policy).
     */
    void stopIph() {
        if (mStream != null && mScrollableContainerDelegate != null
                && mHeaderIphScrollListener != null) {
            if (mHeaderIphScrollListener != null) {
                mScrollableContainerDelegate.removeScrollListener(mHeaderIphScrollListener);
                mHeaderIphScrollListener = null;
            }
            if (mRefreshIphScrollListener != null) {
                mScrollableContainerDelegate.removeScrollListener(mRefreshIphScrollListener);
                mRefreshIphScrollListener = null;
            }
        }
        mScrollableContainerDelegate = null;
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
                .hasPrimaryAccount();
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
        return mSwipeRefreshLayout.canScrollVertically(-1);
    }

    private boolean isReliabilityLoggingEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_RELIABILITY_LOGGING)
                && (mPrivacyPreferencesManager.isMetricsReportingEnabled()
                        || CommandLine.getInstance().hasSwitch(
                                "force-enable-feed-reliability-logging"));
    }
}
