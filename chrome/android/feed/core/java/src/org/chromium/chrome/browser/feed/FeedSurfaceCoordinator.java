// Copyright 2018 The Chromium Authors
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
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.CommandLine;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feature_guide.notifications.FeatureNotificationUtils;
import org.chromium.chrome.browser.feature_guide.notifications.FeatureType;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;
import org.chromium.chrome.browser.feed.sections.SectionHeaderListProperties;
import org.chromium.chrome.browser.feed.sections.SectionHeaderView;
import org.chromium.chrome.browser.feed.sections.SectionHeaderViewBinder;
import org.chromium.chrome.browser.feed.sections.StickySectionHeaderView;
import org.chromium.chrome.browser.feed.settings.FeedAutoplaySettingsFragment;
import org.chromium.chrome.browser.feed.sort_ui.FeedOptionsCoordinator;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.feed.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.feed.FeedLaunchReliabilityLogger.SurfaceType;
import org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScope;
import org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;
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
        implements FeedSurfaceProvider, FeedBubbleDelegate, SwipeRefreshLayout.OnRefreshListener,
                   BackToTopBubbleScrollListener.ResultHandler, SurfaceCoordinator,
                   FeedAutoplaySettingsDelegate, HasContentListener, FeedContentFirstLoadWatcher {
    private static final String TAG = "FeedSurfaceCoordinator";
    private static final long DELAY_FEED_HEADER_IPH_MS = 50;

    protected final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    @Nullable
    private final View mNtpHeader;
    private final boolean mShowDarkBackground;
    private final boolean mIsPlaceholderShownInitially;
    private final FeedSurfaceDelegate mDelegate;
    private final FeedSurfaceMediator mMediator;
    private final BottomSheetController mBottomSheetController;
    private final WindowAndroid mWindowAndroid;
    private final Supplier<ShareDelegate> mShareSupplier;
    private final Handler mHandler;
    private final boolean mOverScrollDisabled;
    private final ObserverList<SurfaceCoordinator.Observer> mObservers = new ObserverList<>();
    private final FeedActionDelegate mActionDelegate;
    private final HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    // FeedReliabilityLogger params.
    private final @SurfaceType int mSurfaceType;
    private final long mEmbeddingSurfaceCreatedTimeNs;

    private UiConfig mUiConfig;
    private FrameLayout mRootView;
    private boolean mIsActive;
    private int mHeaderCount;
    private int mSectionHeaderIndex;
    private int mToolbarHeight;

    // Used when Feed is enabled.
    private @Nullable Profile mProfile;
    private @Nullable FeedSurfaceLifecycleManager mFeedSurfaceLifecycleManager;
    private @Nullable View mSigninPromoView;
    private @Nullable FeedStreamViewResizer mStreamViewResizer;
    // Feed header fields.
    private @Nullable PropertyModel mSectionHeaderModel;
    private @Nullable ViewGroup mViewportView;
    private SectionHeaderView mSectionHeaderView;
    private @Nullable ListModelChangeProcessor<PropertyListModel<PropertyModel, PropertyKey>,
            SectionHeaderView, PropertyKey> mSectionHeaderListModelChangeProcessor;
    private @Nullable PropertyModelChangeProcessor<PropertyModel, SectionHeaderView, PropertyKey>
            mSectionHeaderModelChangeProcessor;
    // Sticky section header fields.
    private @Nullable StickySectionHeaderView mStickySectionHeaderView;
    private @Nullable ListModelChangeProcessor<PropertyListModel<PropertyModel, PropertyKey>,
            SectionHeaderView, PropertyKey> mStickySectionHeaderListModelChangeProcessor;
    private @Nullable PropertyModelChangeProcessor<PropertyModel, SectionHeaderView, PropertyKey>
            mStickySectionHeaderModelChangeProcessor;
    // Feed RecyclerView/xSurface fields.
    private @Nullable FeedListContentManager mContentManager;
    private @Nullable RecyclerView mRecyclerView;
    private @Nullable FeedSurfaceScope mSurfaceScope;
    private @Nullable FeedSurfaceScopeDependencyProviderImpl mDependencyProvider;
    private @Nullable HybridListRenderer mHybridListRenderer;

    // Used to handle things related to the main scrollable container of NTP surface.
    // In start surface, it does not track scrolling events - only the header offset.
    // In New Tab Page, it does not track the header offset (no header) - instead, it
    // tracks scrolling events.
    private @Nullable ScrollableContainerDelegate mScrollableContainerDelegate;

    private @Nullable HeaderIphScrollListener mHeaderIphScrollListener;
    private @Nullable RefreshIphScrollListener mRefreshIphScrollListener;
    private @Nullable StartSurfaceScrollListener mStartSurfaceScrollListener;
    private @Nullable BackToTopBubbleScrollListener mBackToTopBubbleScrollListener;
    private @Nullable FeedReliabilityLogger mReliabilityLogger;
    private final PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;

    private final Supplier<Toolbar> mToolbarSupplier;

    private FeedSwipeRefreshLayout mSwipeRefreshLayout;

    private BackToTopBubble mBackToTopBubble;

    private boolean mWebFeedHasContent;

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

    // This listener is used to handle the sticky header on the start surface.
    class StartSurfaceScrollListener implements ScrollListener {
        @Override
        public void onScrollStateChanged(@ScrollState int state) {}

        @Override
        public void onScrolled(int dx, int dy) {}

        /**
         * On the start surface, the header offset changes will trigger this third method in the
         * listener, and we compare the toolbar height which is passed as a parameter in the
         * constructor with the in-feed header current position. If the header position is smaller
         * than the toolbar height, we make the sticky header visible.
         */
        @Override
        public void onHeaderOffsetChanged(int verticalOffset) {
            boolean isStickyHeaderVisibleOnStartSurface = mToolbarHeight >= getFeedHeaderPosition();
            mSectionHeaderModel.set(SectionHeaderListProperties.STICKY_HEADER_VISIBLILITY_KEY,
                    isStickyHeaderVisibleOnStartSurface);
        }
    }

    private class Scroller implements Runnable {
        @Override
        public void run() {
            // The feed header may not be visible for smaller screens or landscape mode. Scroll
            // to show the header after showing the IPH.
            mMediator.scrollToViewIfNecessary(getSectionHeaderPosition());
        }
    }

    // Returns the index of the section header (for you and following tab header).
    private int getSectionHeaderPosition() {
        return mSectionHeaderIndex;
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
     * @param surfaceType Type of UI surface embedding the feed. Used for reliability logging.
     * @param embeddingSurfaceCreatedTimeNs Timestamp of creation of the UI surface.
     * @param swipeRefreshLayout The layout to support pull-to-refresh.
     * @param overScrollDisabled Whether the overscroll effect is disabled.
     * @param viewportView The view that should be used as a container for viewport measurement
     *   purposes, or |null| if the view returned by HybridListRenderer is to be used.
     * @param actionDelegate Implements some Feed actions.
     * @param helpAndFeedbackLauncher A HelpAndFeedbackLauncher.
     * @param tabModelSelector TabModelSelector used to get TabModels we can observe.
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
            @NonNull Supplier<Toolbar> toolbarSupplier, @SurfaceType int surfaceType,
            long embeddingSurfaceCreatedTimeNs, @Nullable FeedSwipeRefreshLayout swipeRefreshLayout,
            boolean overScrollDisabled, @Nullable ViewGroup viewportView,
            FeedActionDelegate actionDelegate, HelpAndFeedbackLauncher helpAndFeedbackLauncher,
            TabModelSelector tabModelSelector) {
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
        mPrivacyPreferencesManager = privacyPreferencesManager;
        mToolbarSupplier = toolbarSupplier;
        mSwipeRefreshLayout = swipeRefreshLayout;
        mOverScrollDisabled = overScrollDisabled;
        mViewportView = viewportView;
        mActionDelegate = actionDelegate;
        mHelpAndFeedbackLauncher = helpAndFeedbackLauncher;
        mSurfaceType = surfaceType;
        mEmbeddingSurfaceCreatedTimeNs = embeddingSurfaceCreatedTimeNs;
        mWebFeedHasContent = false;
        mSectionHeaderIndex = 0;
        mToolbarHeight = toolbarHeight;

        Resources resources = mActivity.getResources();

        mRootView = new RootView(mActivity);
        mRootView.setPadding(0, resources.getDimensionPixelOffset(R.dimen.tab_strip_height), 0, 0);
        mUiConfig = new UiConfig(mRootView);
        mRecyclerView = setUpView();
        mStreamViewResizer =
                FeedStreamViewResizer.createAndAttach(mActivity, mRecyclerView, mUiConfig);

        // Pull-to-refresh set up.
        if (mSwipeRefreshLayout != null && mSwipeRefreshLayout.getParent() == null) {
            mSwipeRefreshLayout.addView(mRecyclerView);
            mRootView.addView(mSwipeRefreshLayout);
        } else {
            mRootView.addView(mRecyclerView);
        }
        if (mSwipeRefreshLayout != null) {
            mSwipeRefreshLayout.addOnRefreshListener(this);
        }

        mHandler = new Handler(Looper.getMainLooper());

        // MVC setup for feed header and sticky header.
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

        FeedOptionsCoordinator optionsCoordinator = new FeedOptionsCoordinator(mActivity);

        mSectionHeaderModel.set(SectionHeaderListProperties.EXPANDING_DRAWER_VIEW_KEY,
                optionsCoordinator.getView());

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_FEED)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_HEADER_STICK_TO_TOP)) {
            mStickySectionHeaderView =
                    (StickySectionHeaderView) LayoutInflater.from(mActivity).inflate(
                            R.layout.new_tab_page_multi_sticky_feed_header, null, false);
            mRootView.addView(mStickySectionHeaderView);

            mStickySectionHeaderModelChangeProcessor = PropertyModelChangeProcessor.create(
                    mSectionHeaderModel, mStickySectionHeaderView, binder);
            mStickySectionHeaderListModelChangeProcessor = new ListModelChangeProcessor<>(
                    mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY),
                    mStickySectionHeaderView, binder);
            mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                    .addObserver(mStickySectionHeaderListModelChangeProcessor);
            mSectionHeaderModel.set(
                    SectionHeaderListProperties.STICKY_HEADER_EXPANDING_DRAWER_VIEW_KEY,
                    optionsCoordinator.getStickyHeaderOptionsView());

            // TODO(b/258073469): update the margin to the correct number
            // If we are on start surface, the sticky header's margin is temporarily set to be 1/2
            // toolbar height; if we are on NTP, the sticky header's margin should be set to the
            // toolbar height.
            if (mSurfaceType == SurfaceType.START_SURFACE) {
                mSectionHeaderModel.set(
                        SectionHeaderListProperties.STICKY_HEADER_MUTABLE_MARGIN_KEY,
                        mToolbarHeight / 2);
                createStartSurfaceScrollListener();
            } else {
                mSectionHeaderModel.set(
                        SectionHeaderListProperties.STICKY_HEADER_MUTABLE_MARGIN_KEY,
                        mToolbarHeight);
            }
        }

        // Mediator should be created before any Stream changes.
        mMediator = new FeedSurfaceMediator(this, mActivity, snapScrollHelper, mSectionHeaderModel,
                getTabIdFromLaunchOrigin(launchOrigin), actionDelegate, optionsCoordinator);

        FeedSurfaceTracker.getInstance().trackSurface(this);

        // Creates streams, initiates content changes.
        mMediator.updateContent();
    }

    int getToolbarHeight() {
        return mToolbarHeight;
    }

    void setToolbarHairlineVisibility(boolean isVisible) {
        Toolbar toolbar = mToolbarSupplier.get();
        // If the ToolbarLayout isn't visible, we shouldn't change the toolbar_hairline to be
        // visible.
        if (toolbar == null || !toolbar.isBrowsingModeToolbarVisible() && isVisible) {
            return;
        }
        toolbar.setBrowsingModeHairlineVisibility(isVisible);
    }

    /**
     * @return the position of the in-feed header, or an error value Integer.MAX_VALUE when
     * mScrollableContainerDelegate isn't initialized successfully, in this case, the sticky header
     * will always be invisible.
     */
    int getFeedHeaderPosition() {
        if (mScrollableContainerDelegate != null) {
            return mScrollableContainerDelegate.getTopPositionRelativeToContainerView(
                    mSectionHeaderView);
        }
        return Integer.MAX_VALUE;
    }

    @Override
    public void hasContentChanged(@StreamKind int kind, boolean hasContent) {
        if (kind == StreamKind.FOLLOWING) {
            mWebFeedHasContent = hasContent;
        }
    }

    private void stopScrollTracking() {
        if (mScrollableContainerDelegate != null) {
            mScrollableContainerDelegate.removeScrollListener(mDependencyProvider);
            mScrollableContainerDelegate = null;
        }
    }

    private void showDiscoverIph() {
        mHandler.postDelayed(() -> {
            // The feed header may not be visible for smaller screens or landscape mode. Scroll to
            // show the header before showing the IPH.
            mMediator.scrollToViewIfNecessary(getSectionHeaderPosition());
            UserEducationHelper helper = new UserEducationHelper(mActivity, mHandler);
            mSectionHeaderView.showHeaderIph(helper);
        }, DELAY_FEED_HEADER_IPH_MS);
    }

    public void maybeShowWebFeedAwarenessIph() {
        if (mWebFeedHasContent && FeedFeatures.shouldUseWebFeedAwarenessIPH()) {
            UserEducationHelper helper = new UserEducationHelper(mActivity, mHandler);
            mSectionHeaderView.showWebFeedAwarenessIph(
                    helper, StreamTabId.FOLLOWING, new Scroller());
        }
    }

    @Override
    public void nonNativeContentLoaded(@StreamKind int kind) {
        // We want to show the web feed IPH on the first load of the FOR_YOU feed.
        if (kind == StreamKind.FOR_YOU) {
            // After the web feed content has loaded, we will know if we have any content, and it is
            // safe to show the IPH.
            maybeShowWebFeedAwarenessIph();
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
        if (mStickySectionHeaderModelChangeProcessor != null) {
            mStickySectionHeaderModelChangeProcessor.destroy();
            mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                    .removeObserver(mStickySectionHeaderListModelChangeProcessor);
        }

        // Destroy mediator after all other related controller/processors are destroyed.
        mMediator.destroy();

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
    public void reload() {
        onRefresh();
    }

    @Override
    public void onRefresh() {
        updateReloadButtonVisibility(/*isReloading=*/true);
        if (mReliabilityLogger != null) {
            mReliabilityLogger.getLaunchLogger().logManualRefresh(
                    SystemClock.elapsedRealtimeNanos());
        }
        mMediator.manualRefresh((Boolean v) -> {
            updateReloadButtonVisibility(/*isReloading=*/false);
            if (mSwipeRefreshLayout == null) return;
            mSwipeRefreshLayout.setRefreshing(false);
        });
        getFeatureEngagementTracker().notifyEvent(EventConstants.FEED_SWIPE_REFRESHED);
    }

    public void nonSwipeRefresh() {
        if (mSwipeRefreshLayout != null) {
            mSwipeRefreshLayout.startRefreshingAtTheBottom();
        }
        onRefresh();
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

    /** @return Whether the placeholder is shown. */
    public boolean isPlaceholderShown() {
        return mMediator.isPlaceholderShown();
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
        FeatureNotificationUtils.registerIPHCallback(
                FeatureType.NTP_SUGGESTION_CARD, this::showDiscoverIph);
    }

    /** Hides the feed. */
    @Override
    public void onSurfaceClosed() {
        if (!FeedSurfaceTracker.getInstance().isStartupCalled()) return;
        mIsActive = false;
        mMediator.onSurfaceClosed();
        FeatureNotificationUtils.unregisterIPHCallback(FeatureType.NTP_SUGGESTION_CARD);
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
        mContentManager = new FeedListContentManager();
        ProcessScope processScope = FeedSurfaceTracker.getInstance().getXSurfaceProcessScope();
        if (processScope != null) {
            mDependencyProvider = new FeedSurfaceScopeDependencyProviderImpl(
                    mActivity, mActivity, mShowDarkBackground);

            mSurfaceScope = processScope.obtainFeedSurfaceScope(mDependencyProvider);
            if (mScrollableContainerDelegate != null) {
                mScrollableContainerDelegate.addScrollListener(mDependencyProvider);
            }
        } else {
            mDependencyProvider = null;
            mSurfaceScope = null;
        }
        if (mSurfaceScope != null) {
            mHybridListRenderer = mSurfaceScope.provideListRenderer();

            if (mPrivacyPreferencesManager.isMetricsReportingEnabled()
                    || CommandLine.getInstance().hasSwitch(
                            "force-enable-feed-reliability-logging")) {
                FeedLaunchReliabilityLogger launchLogger =
                        mSurfaceScope.getLaunchReliabilityLogger();
                FeedUserInteractionReliabilityLogger userInteractionLogger = null;
                if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.FEED_USER_INTERACTION_RELIABILITY_REPORT)) {
                    userInteractionLogger = mSurfaceScope.getUserInteractionReliabilityLogger();
                }
                mReliabilityLogger = new FeedReliabilityLogger(launchLogger, userInteractionLogger);
                launchLogger.logUiStarting(mSurfaceType, mEmbeddingSurfaceCreatedTimeNs);
            }

        } else {
            mHybridListRenderer = new NativeViewListRenderer(mActivity);
        }

        RecyclerView view;
        if (mHybridListRenderer != null) {
            // XSurface returns a View, but it should be a RecyclerView.
            boolean useStaggeredLayout = FeedFeatures.isMultiColumnFeedEnabled(mActivity);
            view = (RecyclerView) mHybridListRenderer.bind(
                    mContentManager, mViewportView, useStaggeredLayout);
            view.setId(R.id.feed_stream_recycler_view);
            view.setClipToPadding(false);
            view.setBackgroundColor(SemanticColorUtils.getDefaultBgColor(mActivity));

            // Work around https://crbug.com/943873 where default focus highlight shows up after
            // toggling dark mode.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                view.setDefaultFocusHighlightEnabled(false);
            }
            if (mOverScrollDisabled) {
                view.setOverScrollMode(View.OVER_SCROLL_NEVER);
            }
        } else {
            view = null;
        }
        return view;
    }

    /** @return The {@link RecyclerView} associated with this feed. */
    public RecyclerView getRecyclerView() {
        return mRecyclerView;
    }

    /** @return The {@link FeedSurfaceScope} used to create this feed. */
    FeedSurfaceScope getSurfaceScope() {
        return mSurfaceScope;
    }

    /** @return The {@link HybridListRenderer} used to render this feed. */
    HybridListRenderer getHybridListRenderer() {
        return mHybridListRenderer;
    }

    /** @return The {@link FeedListContentManager} managing the contents of this feed. */
    FeedListContentManager getContentManager() {
        return mContentManager;
    }

    /**
     * @return This surface's {@link FeedReliabilityLogger}.
     */
    @Override
    public FeedReliabilityLogger getReliabilityLogger() {
        return mReliabilityLogger;
    }

    /**
     * Configures header views and properties for feed:
     * Adds the feed headers, creates the feed lifecycle manager, adds swipe-to-refresh if needed.
     */
    void setupHeaders(boolean feedEnabled) {
        // Directly add header views to content manager.
        List<View> headerList = new ArrayList<>();
        if (mNtpHeader != null) {
            headerList.add(mNtpHeader);
        }

        if (feedEnabled) {
            mActionDelegate.onStreamCreated();
            mFeedSurfaceLifecycleManager = mDelegate.createStreamLifecycleManager(mActivity, this);
            headerList.add(mSectionHeaderView);
            if (mSwipeRefreshLayout != null) {
                mSwipeRefreshLayout.enableSwipe(mScrollableContainerDelegate);
            }
        } else {
            if (mFeedSurfaceLifecycleManager != null) {
                mFeedSurfaceLifecycleManager.destroy();
                mFeedSurfaceLifecycleManager = null;
            }
            if (mSwipeRefreshLayout != null) {
                mSwipeRefreshLayout.disableSwipe();
            }
        }
        setHeaders(headerList);

        // Explicitly request focus on the scroll container to avoid UrlBar being focused after
        // mRootView containers are refreshed.
        mRecyclerView.requestFocus();
    }

    /**
     * Creates a flavor {@Link FeedStream} without any other side-effects.
     *
     * @param kind Kind of stream being created.
     * @return The FeedStream created.
     */
    FeedStream createFeedStream(@StreamKind int kind, Stream.StreamsMediator streamsMediator) {
        return new FeedStream(mActivity, mSnackbarManager, mBottomSheetController,
                mIsPlaceholderShownInitially, mWindowAndroid, mShareSupplier, kind, this,
                mActionDelegate, mHelpAndFeedbackLauncher, this /* FeedContentFirstLoadWatcher */,
                streamsMediator, null);
    }

    private void setHeaders(List<View> headerViews) {
        // Build the list of headers we want, and then replace existing headers.
        List<FeedListContentManager.FeedContent> headerList = new ArrayList<>();
        for (View header : headerViews) {
            // Feed header view in multi does not need padding added.
            int lateralPaddingsPx = getLateralPaddingsPx();
            if (header == mSectionHeaderView) {
                lateralPaddingsPx = 0;
            }

            FeedListContentManager.NativeViewContent content =
                    new FeedListContentManager.NativeViewContent(
                            lateralPaddingsPx, "Header" + header.hashCode(), header);
            headerList.add(content);
        }
        if (mContentManager.replaceRange(0, mHeaderCount, headerList)) {
            mHeaderCount = headerList.size();
            mMediator.notifyHeadersChanged(mHeaderCount);
        }
        // The section header is the last header to be added, save its index.
        mSectionHeaderIndex = headerViews.size() - 1;
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
                    R.layout.sync_promo_view_content_suggestions, mRootView, false);
        }
        return mSigninPromoView;
    }

    /**
     * Update header views in the Feed.
     */
    void updateHeaderViews(boolean isSignInPromoVisible) {
        if (!mMediator.hasStreams()) return;

        List<View> headers = new ArrayList<>();
        if (mNtpHeader != null) {
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
        if (!mMediator.hasStreams()) return;

        // Provide a delegate for the container of the feed surface that is handled by the feed
        // coordinator itself when not provided externally (e.g., by the NewTabPage).
        if (mScrollableContainerDelegate == null) {
            mScrollableContainerDelegate = new ScrollableContainerDelegateImpl();
        }

        createHeaderIphScrollListener();
        createRefreshIphScrollListener();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_BACK_TO_TOP)) {
            createBackToTopBubbleScrollListener();
        }
    }

    private void createStartSurfaceScrollListener() {
        mStartSurfaceScrollListener = new StartSurfaceScrollListener();
        mScrollableContainerDelegate.addScrollListener(mStartSurfaceScrollListener);
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
        if (mMediator.hasStreams() && mScrollableContainerDelegate != null) {
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
        return FeedServiceBridge.isSignedIn();
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
        return mMediator.getLastFetchTimeMsForCurrentStream();
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
        return mRecyclerView.getLayoutManager().getItemCount();
    }

    @Override
    public int getFirstVisiblePosition() {
        return mHybridListRenderer.getListLayoutHelper().findFirstVisibleItemPosition();
    }

    @Override
    public int getLastVisiblePosition() {
        return mHybridListRenderer.getListLayoutHelper().findLastVisibleItemPosition();
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

    @Override
    public void onActivityPaused() {
        if (mReliabilityLogger != null) {
            mReliabilityLogger.onActivityPaused();
        }
    }

    @Override
    public void onActivityResumed() {
        if (mReliabilityLogger != null) {
            mReliabilityLogger.onActivityResumed();
        }
    }

    public boolean isLoadingFeed() {
        return mMediator.isLoadingFeed();
    }

    private int getLateralPaddingsPx() {
        return mActivity.getResources().getDimensionPixelSize(
                R.dimen.ntp_header_lateral_paddings_v2);
    }

    @VisibleForTesting
    public void setReliabilityLoggerForTesting(FeedReliabilityLogger logger) {
        mReliabilityLogger = logger;
    }

    @VisibleForTesting
    public void clearScrollableContainerDelegateForTesting() {
        mScrollableContainerDelegate = null;
    }

    @VisibleForTesting
    public FeedActionDelegate getActionDelegateForTesting() {
        return mActionDelegate;
    }
}
