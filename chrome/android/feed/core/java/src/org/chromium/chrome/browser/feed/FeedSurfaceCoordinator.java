// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.browser_ui.styles.SemanticColorUtils.getDefaultIconColor;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;
import androidx.core.widget.ImageViewCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;
import org.chromium.chrome.browser.feed.sections.SectionHeaderListProperties;
import org.chromium.chrome.browser.feed.sections.SectionHeaderView;
import org.chromium.chrome.browser.feed.sections.SectionHeaderViewBinder;
import org.chromium.chrome.browser.feed.sort_ui.FeedOptionsCoordinator;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinatorFactory;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationMetricsUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.NtpBackgroundImageCoordinator;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface.feed.FeedCardOpeningReliabilityLogger;
import org.chromium.chrome.browser.xsurface.feed.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.feed.FeedLaunchReliabilityLogger.SurfaceType;
import org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScope;
import org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger;
import org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger.ClosedReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Provides a surface that displays an interest feed rendered list of content suggestions. */
@NullMarked
public class FeedSurfaceCoordinator
        implements FeedSurfaceProvider,
                FeedBubbleDelegate,
                SwipeRefreshLayout.OnRefreshListener,
                SurfaceCoordinator,
                HasContentListener,
                FeedContentFirstLoadWatcher {
    @Nullable ImageView getRecyclerViewSnapshotOverlayForTesting() {
        return mRecyclerViewSnapshotOverlay;
    }

    protected final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    private final @Nullable View mNtpHeader;
    private final boolean mShowDarkBackground;
    private final FeedSurfaceDelegate mDelegate;
    private final BottomSheetController mBottomSheetController;
    private final WindowAndroid mWindowAndroid;
    private final Supplier<ShareDelegate> mShareSupplier;
    private final Handler mHandler;
    private final boolean mOverScrollDisabled;
    private final ObserverList<SurfaceCoordinator.Observer> mObservers = new ObserverList<>();
    private final FeedActionDelegate mActionDelegate;
    private final boolean mUseStaggeredLayout;
    private final int mDefaultBackgroundColor;

    // FeedReliabilityLogger params.
    private final long mEmbeddingSurfaceCreatedTimeNs;

    private FeedSurfaceMediator mMediator;
    private final boolean mIsNtpCustomizationV2Enabled;
    private final UiConfig mUiConfig;
    private final FrameLayout mRootView;
    private boolean mIsActive;
    private int mHeaderCount;
    private int mHeaderIndex;
    private final View mHeaderView;

    // Used when Feed is enabled.
    private final Profile mProfile;
    private @Nullable FeedSurfaceLifecycleManager mFeedSurfaceLifecycleManager;
    private @Nullable View mSigninPromoView;
    // Feed header fields.
    private @Nullable PropertyModel mSectionHeaderModel;
    private final @Nullable ViewGroup mViewportView;
    private @Nullable
            ListModelChangeProcessor<
                    PropertyListModel<PropertyModel, PropertyKey>, SectionHeaderView, PropertyKey>
            mSectionHeaderListModelChangeProcessor;
    private @Nullable PropertyModelChangeProcessor<PropertyModel, SectionHeaderView, PropertyKey>
            mSectionHeaderModelChangeProcessor;
    // Feed RecyclerView/xSurface fields.
    private FeedListContentManager mContentManager;
    private final RecyclerView mRecyclerView;
    private @Nullable ImageView mRecyclerViewSnapshotOverlay;
    private @Nullable FeedSurfaceScope mSurfaceScope;
    private @Nullable FeedSurfaceScopeDependencyProviderImpl mDependencyProvider;
    private HybridListRenderer mHybridListRenderer;

    // Used to handle things related to the main scrollable container of NTP surface.
    // In start surface, it does not track scrolling events - only the header offset.
    // In New Tab Page, it does not track the header offset (no header) - instead, it
    // tracks scrolling events.
    private @Nullable ScrollableContainerDelegate mScrollableContainerDelegate;

    private @Nullable HeaderIphScrollListener mHeaderIphScrollListener;
    private @Nullable RefreshIphScrollListener mRefreshIphScrollListener;
    private @Nullable FeedReliabilityLogger mReliabilityLogger;
    private final PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;

    private final Supplier<Toolbar> mToolbarSupplier;

    private FeedSwipeRefreshLayout mSwipeRefreshLayout;

    private boolean mWebFeedHasContent;
    private final ObservableSupplier<Integer> mTabStripHeightSupplier;
    private final Callback<Integer> mTabStripHeightChangeCallback;

    // Used to handle padding adjustment when edge to edge is enabled.
    private final EdgeToEdgePadAdjuster mEdgePadAdjuster;
    private final boolean mIsNewTabPageCustomizationEnabled;
    private @Nullable ImageButton mNtpCustomizationButton;
    private @Nullable NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    private @Nullable NtpBackgroundImageCoordinator mNtpBackgroundImageCoordinator;
    private NtpCustomizationConfigManager.@Nullable HomepageStateListener mHomepageStateListener;

    /** Provides the additional capabilities needed for the container view. */
    private class RootView extends FrameLayout {
        /**
         * @param context The context of the application.
         */
        RootView(Context context) {
            super(context);
        }

        @Override
        protected void onConfigurationChanged(Configuration newConfig) {
            super.onConfigurationChanged(newConfig);
            mUiConfig.updateDisplayStyle();
        }

        @Override
        protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
            super.onSizeChanged(width, height, oldWidth, oldHeight);
            if (oldWidth != 0 && oldHeight != 0 && mRecyclerViewSnapshotOverlay != null) {
                // TODO(crbug.com/451422517): This is a temporary solution to make resizing on
                // large screen devices smoother. Remove this once the long term solution is
                // implemented.
                handleResize(width, height);
            }
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CONTAINMENT)) {
                mRecyclerView.post(mRecyclerView::invalidateItemDecorations);
                updateNtpHeaderMargins();
            }
            if (mIsNewTabPageCustomizationEnabled && mUseStaggeredLayout) {
                updateNtpCustomizationButtonVisibility();
            }
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent ev) {
            // This is for caching the last touch position on the NTP. {@link
            // ViewGroup#onInterceptTouchEvent} is called for every motion event that will be
            // consumed by this view or any of its child views. This location allows us to cache
            // the last touch before any views actually perform an intercept or handle the touch
            // event. Placing this call later in the method would mean at least a subset of events
            // would be missed.
            mDelegate.sendMotionEventForInputTracking(ev);

            if (super.onInterceptTouchEvent(ev)) return true;
            if (mMediator != null && !mMediator.getTouchEnabled()) return true;

            return mDelegate.onInterceptTouchEvent(ev);
        }

        @Override
        public void onMeasure(int x, int y) {
            try (TraceEvent e = TraceEvent.scoped("Feed.RootView.onMeasure")) {
                super.onMeasure(x, y);
            }
        }

        @Override
        public void onLayout(boolean a, int b, int c, int d, int e) {
            try (TraceEvent e1 = TraceEvent.scoped("Feed.RootView.onLayout")) {
                super.onLayout(a, b, c, d, e);
            }
        }

        @Override
        public void onDraw(android.graphics.Canvas canvas) {
            try (TraceEvent e = TraceEvent.scoped("Feed.RootView.onDraw")) {
                super.onDraw(canvas);
            }
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

    // TracingAndPerfScrollListener is explicitly not a ScrollListener due to the fact that the
    // ScrollableContainerDelegate could be null if we are tracking scrolling. However for looking
    // at performance metrics of scrolling we always want to know when feed is scrolling.
    static class TracingAndPerfScrollListener extends RecyclerView.OnScrollListener {
        @Override
        public void onScrollStateChanged(RecyclerView view, int newState) {
            switch (mPrevState) {
                case -1:
                    {
                        if (newState == RecyclerView.SCROLL_STATE_DRAGGING) {
                            startScroll();
                        } else if (newState == RecyclerView.SCROLL_STATE_SETTLING) {
                            startFling();
                        }
                        // else IDLE
                        break;
                    }
                case RecyclerView.SCROLL_STATE_IDLE:
                    {
                        if (newState == RecyclerView.SCROLL_STATE_DRAGGING) {
                            startScroll();
                        } else if (newState == RecyclerView.SCROLL_STATE_SETTLING) {
                            startFling();
                        }
                        break;
                    }
                case RecyclerView.SCROLL_STATE_DRAGGING:
                    {
                        endScroll();
                        if (newState == RecyclerView.SCROLL_STATE_SETTLING) {
                            startFling();
                        }
                        break;
                    }
                case RecyclerView.SCROLL_STATE_SETTLING:
                    {
                        endFling();
                        if (newState == RecyclerView.SCROLL_STATE_DRAGGING) {
                            startScroll();
                        }
                        break;
                    }
                default:
                    {
                        mPrevState = -1;
                        break;
                    }
            }
            mPrevState = newState;
        }

        @Override
        public void onScrolled(RecyclerView view, int dx, int dy) {}

        private void startScroll() {
            // TODO(nuskos): These next two are just a "hack" to get a nice track name
            // in the UI (it uses the first event it hits). Eventually with the Perfetto
            // SDK we could just explicitly title the track instead.
            TraceEvent.startAsync("Feed.ScrollState", hashCode());
            TraceEvent.finishAsync("Feed.ScrollState", hashCode());
            TraceEvent.startAsync("Feed.TouchScrollStarted", hashCode());
        }

        private void endScroll() {
            TraceEvent.finishAsync("Feed.TouchScrollEnded", hashCode());
        }

        private void startFling() {
            TraceEvent.startAsync("Feed.FlingScrollStarted", hashCode());
        }

        private void endFling() {
            TraceEvent.finishAsync("Feed.FlingScrollEnded", hashCode());
        }

        private int mPrevState = -1;
    }

    private class Scroller implements Runnable {
        @Override
        public void run() {
            // The feed header may not be visible for smaller screens or landscape mode. Scroll
            // to show the header after showing the IPH.
            mMediator.scrollToViewIfNecessary(getHeaderPosition());
        }
    }

    // Returns the index of the header if it is visible. Otherwise returns the index after the
    // invisible header.
    int getHeaderPosition() {
        return mHeaderIndex + (mHeaderView.getVisibility() != View.VISIBLE ? 1 : 0);
    }

    boolean useStaggeredLayout() {
        return mUseStaggeredLayout;
    }

    /**
     * Constructs a new FeedSurfaceCoordinator.
     *
     * @param activity The containing {@link Activity}.
     * @param snackbarManager The {@link SnackbarManager} displaying Snackbar UI.
     * @param windowAndroid The window of the page.
     * @param snapScrollHelper The {@link SnapScrollHelper} for the New Tab Page.
     * @param ntpHeader The extra header on top of the feeds for the New Tab Page.
     * @param toolbarHeight The height of the toolbar which overlaps Feed content at the top of the
     *     view.
     * @param showDarkBackground Whether is shown on dark background.
     * @param delegate The constructing {@link FeedSurfaceDelegate}.
     * @param profile The current user profile.
     * @param bottomSheetController The bottom sheet controller.
     * @param shareDelegateSupplier The supplier for the share delegate used to share articles.
     * @param launchOrigin The origin of what launched the feed.
     * @param privacyPreferencesManager Manages the privacy preferences.
     * @param toolbarSupplier Supplies the {@link Toolbar}.
     * @param embeddingSurfaceCreatedTimeNs Timestamp of creation of the UI surface.
     * @param swipeRefreshLayout The layout to support pull-to-refresh.
     * @param overScrollDisabled Whether the overscroll effect is disabled.
     * @param viewportView The view that should be used as a container for viewport measurement
     *     purposes, or |null| if the view returned by HybridListRenderer is to be used.
     * @param actionDelegate Implements some Feed actions.
     * @param tabStripHeightSupplier Supplier for the tab strip height.
     * @param edgeToEdgeControllerSupplier Supplier for the {@link EdgeToEdgeController} instance.
     */
    public FeedSurfaceCoordinator(
            Activity activity,
            SnackbarManager snackbarManager,
            WindowAndroid windowAndroid,
            @Nullable SnapScrollHelper snapScrollHelper,
            @Nullable View ntpHeader,
            @Px int toolbarHeight,
            boolean showDarkBackground,
            FeedSurfaceDelegate delegate,
            Profile profile,
            BottomSheetController bottomSheetController,
            Supplier<ShareDelegate> shareDelegateSupplier,
            @Nullable ScrollableContainerDelegate externalScrollableContainerDelegate,
            @NewTabPageLaunchOrigin int launchOrigin,
            PrivacyPreferencesManagerImpl privacyPreferencesManager,
            Supplier<Toolbar> toolbarSupplier,
            long embeddingSurfaceCreatedTimeNs,
            FeedSwipeRefreshLayout swipeRefreshLayout,
            boolean overScrollDisabled,
            @Nullable ViewGroup viewportView,
            FeedActionDelegate actionDelegate,
            ObservableSupplier<Integer> tabStripHeightSupplier,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mNtpHeader = ntpHeader;
        mShowDarkBackground = showDarkBackground;
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
        mEmbeddingSurfaceCreatedTimeNs = embeddingSurfaceCreatedTimeNs;
        mWebFeedHasContent = false;
        mHeaderIndex = 0;
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mUseStaggeredLayout = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity);
        mIsNewTabPageCustomizationEnabled = ChromeFeatureList.sNewTabPageCustomization.isEnabled();
        mDefaultBackgroundColor =
                ContextCompat.getColor(mActivity, R.color.home_surface_background_color);

        mRootView = new RootView(mActivity);
        mRootView.setPadding(0, mTabStripHeightSupplier.get(), 0, 0);

        mTabStripHeightChangeCallback =
                newHeight ->
                        mRootView.setPadding(
                                mRootView.getPaddingLeft(),
                                newHeight,
                                mRootView.getPaddingRight(),
                                mRootView.getPaddingBottom());
        mTabStripHeightSupplier.addObserver(mTabStripHeightChangeCallback);

        mUiConfig = new UiConfig(mRootView);
        mRecyclerView = setUpView();
        FeedStreamViewResizer.createAndAttach(mActivity, mRecyclerView, mUiConfig);

        mIsNtpCustomizationV2Enabled = ChromeFeatureList.sNewTabPageCustomizationV2.isEnabled();
        if (mIsNewTabPageCustomizationEnabled) {
            mNtpBackgroundImageCoordinator =
                    new NtpBackgroundImageCoordinator(
                            mActivity, mRootView, mUiConfig, mDefaultBackgroundColor);
        }

        // Pull-to-refresh set up.
        if (mSwipeRefreshLayout != null && mSwipeRefreshLayout.getParent() == null) {
            mSwipeRefreshLayout.addView(mRecyclerView);
            mRootView.addView(mSwipeRefreshLayout);
        } else {
            mRootView.addView(mRecyclerView);
        }
        // TODO(crbug.com/451422517): This is a temporary solution to prevent NTP flashing.
        // The snapshot overlay is added to the RootView to cover the RecyclerView during resize.
        if (ChromeFeatureList.sFluidResize.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            mRecyclerViewSnapshotOverlay = new ImageView(mActivity);
            mRecyclerViewSnapshotOverlay.setVisibility(View.GONE);
            mRecyclerViewSnapshotOverlay.setScaleType(ImageView.ScaleType.FIT_START);
            mRootView.addView(mRecyclerViewSnapshotOverlay);
        }
        if (mSwipeRefreshLayout != null) {
            mSwipeRefreshLayout.addOnRefreshListener(this);
        }

        // The NTP customization button needs to be added after the RecyclerView to make it float
        // above the RecyclerView.
        if (mIsNewTabPageCustomizationEnabled && mUseStaggeredLayout) {
            mNtpCustomizationButton = new ImageButton(mActivity);
            mNtpCustomizationButton.setImageResource(R.drawable.bookmark_edit_active);
            mNtpCustomizationButton.setBackgroundResource(R.drawable.edit_icon_circle_background);
            ImageViewCompat.setImageTintList(
                    mNtpCustomizationButton,
                    ColorStateList.valueOf(getDefaultIconColor(mRootView.getContext())));
            int size =
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(
                                    R.dimen.ntp_customization_edit_icon_background_size);
            FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(size, size);
            layoutParams.gravity = Gravity.BOTTOM | Gravity.END;
            int margin =
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.ntp_customization_button_margin);
            layoutParams.setMargins(0, 0, margin, margin);
            mNtpCustomizationButton.setLayoutParams(layoutParams);
            mNtpCustomizationButton.setOnClickListener(
                    v -> {
                        showNtpCustomizationBottomSheet();
                    });
            mNtpCustomizationButton.setContentDescription(
                    mActivity.getString(R.string.ntp_customization_title));
            mRootView.addView(mNtpCustomizationButton);
        }

        if (mIsNtpCustomizationV2Enabled) {
            mNtpCustomizationConfigManager = NtpCustomizationConfigManager.getInstance();
            mHomepageStateListener =
                    new NtpCustomizationConfigManager.HomepageStateListener() {
                        @Override
                        public void onBackgroundImageChanged(
                                Bitmap originalBitmap,
                                @Nullable BackgroundImageInfo backgroundImageInfo,
                                boolean fromInitialization,
                                @NtpBackgroundImageType int oldType,
                                @NtpBackgroundImageType int newType) {
                            setBackground(originalBitmap, backgroundImageInfo, newType);
                        }

                        @Override
                        public void onBackgroundColorChanged(
                                @Nullable NtpThemeColorInfo ntpThemeColorInfo,
                                @ColorInt int backgroundColor,
                                boolean fromInitialization,
                                @NtpBackgroundImageType int oldType,
                                @NtpBackgroundImageType int newType) {
                            setBackgroundColor(backgroundColor);
                        }
                    };

            mNtpCustomizationConfigManager.addListener(mHomepageStateListener, activity);
        } else {
            setBackgroundColor(mDefaultBackgroundColor);
        }

        mHandler = new Handler(Looper.getMainLooper());

        // MVC setup for feed header.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_HEADER_REMOVAL)) {
            String treatment =
                    ChromeFeatureList.getFieldTrialParamByFeature(
                            ChromeFeatureList.FEED_HEADER_REMOVAL, "treatment");
            mHeaderView =
                    LayoutInflater.from(mActivity)
                            .inflate(R.layout.new_tab_page_feed_header, null, false);
            mHeaderView.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
            if (!treatment.equals("label")) {
                mHeaderView.setVisibility(View.GONE);
            }
        } else {
            if (WebFeedBridge.isWebFeedEnabled()) {
                mHeaderView =
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.new_tab_page_multi_feed_header, null, false);
            } else {
                mHeaderView =
                        LayoutInflater.from(mActivity)
                                .inflate(
                                        R.layout.new_tab_page_feed_v2_expandable_header,
                                        null,
                                        false);
            }
        }

        FeedOptionsCoordinator optionsCoordinator = new FeedOptionsCoordinator(mActivity);

        if (mHeaderView != null && mHeaderView instanceof SectionHeaderView) {
            mSectionHeaderModel = SectionHeaderListProperties.create(toolbarHeight);

            SectionHeaderViewBinder binder = new SectionHeaderViewBinder();
            mSectionHeaderModelChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mSectionHeaderModel, (SectionHeaderView) mHeaderView, binder);
            mSectionHeaderListModelChangeProcessor =
                    new ListModelChangeProcessor<>(
                            mSectionHeaderModel.get(
                                    SectionHeaderListProperties.SECTION_HEADERS_KEY),
                            (SectionHeaderView) mHeaderView,
                            binder);
            mSectionHeaderModel
                    .get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                    .addObserver(mSectionHeaderListModelChangeProcessor);

            mSectionHeaderModel.set(
                    SectionHeaderListProperties.EXPANDING_DRAWER_VIEW_KEY,
                    optionsCoordinator.getView());
        }

        if (mNtpHeader != null && ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CONTAINMENT)) {
            int bottomPadding =
                    mActivity.getResources().getDimensionPixelSize(R.dimen.feed_header_top_margin);
            mNtpHeader.setPadding(
                    mNtpHeader.getPaddingLeft(),
                    mNtpHeader.getPaddingTop(),
                    mNtpHeader.getPaddingRight(),
                    bottomPadding);

            updateNtpHeaderMargins();
        }

        // Mediator should be created before any Stream changes.
        boolean useUiConfig = ntpHeader != null && mUseStaggeredLayout;
        mMediator =
                new FeedSurfaceMediator(
                        this,
                        mActivity,
                        snapScrollHelper,
                        mSectionHeaderModel,
                        getTabIdFromLaunchOrigin(launchOrigin),
                        actionDelegate,
                        optionsCoordinator,
                        useUiConfig ? mUiConfig : null,
                        profile);

        FeedSurfaceTracker.getInstance().trackSurface(this);

        // Set up edge to edge
        mEdgePadAdjuster =
                EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                        mRecyclerView, edgeToEdgeControllerSupplier);

        // Creates streams, initiates content changes.
        mMediator.updateContent();
    }

    private void handleResize(int newWidth, int newHeight) {
        if (mRecyclerViewSnapshotOverlay == null) return;
        Bitmap snapshot = takeRecyclerViewSnapshot(newWidth, newHeight);
        if (snapshot == null) return;

        mRecyclerViewSnapshotOverlay.setImageBitmap(snapshot);

        mRecyclerViewSnapshotOverlay.setVisibility(View.VISIBLE);
        mRecyclerView.setVisibility(View.INVISIBLE);

        mHandler.post(
                () -> {
                    if (mRecyclerViewSnapshotOverlay == null) return;
                    mRecyclerView.setVisibility(View.VISIBLE);
                    mRecyclerViewSnapshotOverlay.setVisibility(View.GONE);
                    // Recycle the bitmap to free up memory immediately.
                    Drawable drawable = mRecyclerViewSnapshotOverlay.getDrawable();
                    if (drawable instanceof BitmapDrawable bitmapDrawable) {
                        Bitmap bitmap = bitmapDrawable.getBitmap();
                        if (bitmap != null) {
                            bitmap.recycle();
                        }
                    }
                    mRecyclerViewSnapshotOverlay.setImageDrawable(null);
                });
    }

    private @Nullable Bitmap takeRecyclerViewSnapshot(int newWidth, int newHeight) {
        int recyclerWidth = newWidth;
        // The recycler view is padded at the top by the tab strip height.
        int recyclerHeight = newHeight - mRootView.getPaddingTop();

        if (recyclerWidth <= 0 || recyclerHeight <= 0) {
            return null;
        }
        try {
            // Manually measure and layout the RecyclerView to the new size.
            mRecyclerView.measure(
                    View.MeasureSpec.makeMeasureSpec(recyclerWidth, View.MeasureSpec.EXACTLY),
                    View.MeasureSpec.makeMeasureSpec(recyclerHeight, View.MeasureSpec.EXACTLY));
            mRecyclerView.layout(0, 0, recyclerWidth, recyclerHeight);

            Bitmap bitmap =
                    Bitmap.createBitmap(recyclerWidth, recyclerHeight, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            ViewUtils.captureBitmap(mRecyclerView, canvas);
            return bitmap;
        } catch (Exception e) {
            return null;
        }
    }

    // Sets the background image for the embedder NTP.
    private void setBackground(
            Bitmap originalBitmap,
            @Nullable BackgroundImageInfo backgroundImageInfo,
            @NtpBackgroundImageType int backgroundType) {
        if (mNtpHeader != null) {
            mNtpHeader.setBackgroundColor(Color.TRANSPARENT);
        }

        mRecyclerView.setBackgroundColor(Color.TRANSPARENT);
        assumeNonNull(mNtpBackgroundImageCoordinator)
                .setBackground(originalBitmap, backgroundImageInfo, backgroundType);
    }

    /**
     * Sets the background color for the embedder NTP.
     *
     * @param backgroundColor The customized background color.
     */
    private void setBackgroundColor(@ColorInt int backgroundColor) {
        mRecyclerView.setBackgroundColor(backgroundColor);
        if (mNtpBackgroundImageCoordinator != null) {
            mNtpBackgroundImageCoordinator.clearBackground();
        }

        if (mNtpHeader != null) {
            if (backgroundColor != mDefaultBackgroundColor) {
                mNtpHeader.setBackgroundColor(Color.TRANSPARENT);
            } else {
                mNtpHeader.setBackgroundColor(mDefaultBackgroundColor);
            }
        }
    }

    void updateNtpHeaderMargins() {
        if (mNtpHeader == null) {
            return;
        }

        // Apply negative margins to the NTP header in order to compensate the containment paddings
        // applied to the whole NTP for non-wide display. This is to allow all the elements in the
        // NTP header to keep using their existing margins/paddings settings.
        int feed_containment_margin =
                mActivity.getResources().getDimensionPixelSize(R.dimen.feed_containment_margin);
        int margin = mUiConfig.getCurrentDisplayStyle().isWide() ? 0 : -feed_containment_margin;
        FrameLayout.LayoutParams layoutParams =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        layoutParams.setMarginStart(margin);
        layoutParams.setMarginEnd(margin);
        mNtpHeader.setLayoutParams(layoutParams);
    }

    void updateNtpCustomizationButtonVisibility() {
        int min_margin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.min_margin_for_ntp_customization_button);
        assumeNonNull(mNtpCustomizationButton);
        mNtpCustomizationButton.setVisibility(
                mRecyclerView.getPaddingLeft() >= min_margin ? View.VISIBLE : View.GONE);
    }

    void showNtpCustomizationBottomSheet() {
        NtpCustomizationCoordinatorFactory.getInstance()
                .create(
                        mActivity,
                        mBottomSheetController,
                        () -> mProfile,
                        NtpCustomizationCoordinator.BottomSheetType.MAIN)
                .showBottomSheet();
        NtpCustomizationMetricsUtils.recordOpenBottomSheetEntry(
                NtpCustomizationCoordinator.EntryPointType.NEW_TAB_PAGE);
        RecordUserAction.record("MobileNewTabPageNtpCustomization");
    }

    @Override
    public void hasContentChanged(@StreamKind int kind, boolean hasContent) {
        if (kind == StreamKind.FOLLOWING) {
            mWebFeedHasContent = hasContent;
        }
    }

    private void stopScrollTracking() {
        if (mScrollableContainerDelegate != null) {
            if (mDependencyProvider != null) {
                mScrollableContainerDelegate.removeScrollListener(mDependencyProvider);
            }
            mScrollableContainerDelegate = null;
        }
    }

    public void maybeShowWebFeedAwarenessIph() {
        if (mHeaderView != null
                && mHeaderView instanceof SectionHeaderView
                && mWebFeedHasContent
                && FeedFeatures.shouldUseWebFeedAwarenessIph()
                && !FeedFeatures.isFeedFollowUiUpdateEnabled()) {
            UserEducationHelper helper = new UserEducationHelper(mActivity, mProfile, mHandler);
            ((SectionHeaderView) mHeaderView)
                    .showWebFeedAwarenessIph(helper, StreamTabId.FOLLOWING, new Scroller());
        }
    }

    @Override
    public void nonNativeContentLoaded(@StreamKind int kind) {
        // We want to show the web feed IPH on the first load of the FOR_YOU feed.
        if (kind == StreamKind.FOR_YOU) {
            // After the web feed content has loaded, we will know if we have any content, and
            // it is safe to show the IPH.
            maybeShowWebFeedAwarenessIph();
        }
    }

    @Override
    @SuppressWarnings("NullAway")
    public void destroy() {
        if (mSwipeRefreshLayout != null) {
            if (mSwipeRefreshLayout.isRefreshing()) {
                mSwipeRefreshLayout.setRefreshing(false);
                updateReloadButtonVisibility(/* isReloading= */ false);
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
            mSectionHeaderModel
                    .get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                    .removeObserver(mSectionHeaderListModelChangeProcessor);
        }

        // Destroy mediator after all other related controller/processors are destroyed.
        mMediator.destroy();

        FeedSurfaceTracker.getInstance().untrackSurface(this);
        if (mHybridListRenderer != null) {
            mHybridListRenderer.unbind();
        }
        mRootView.removeAllViews();
        mTabStripHeightSupplier.removeObserver(mTabStripHeightChangeCallback);
        if (mEdgePadAdjuster != null) {
            mEdgePadAdjuster.destroy();
        }

        if (mNtpCustomizationConfigManager != null) {
            mNtpCustomizationConfigManager.removeListener(mHomepageStateListener);
            mHomepageStateListener = null;
        }
        if (mNtpBackgroundImageCoordinator != null) {
            mNtpBackgroundImageCoordinator.destroy();
        }
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
        manualRefresh();
    }

    /**
     * Implements SwipeRefreshLayout.OnRefreshListener to be used only for pull
     * to refresh.
     */
    @Override
    public void onRefresh() {
        manualRefresh();
        getFeatureEngagementTracker().notifyEvent(EventConstants.FEED_SWIPE_REFRESHED);
    }

    public void nonSwipeRefresh() {
        if (mSwipeRefreshLayout != null) {
            mSwipeRefreshLayout.startRefreshingAtTheBottom();
        }
        manualRefresh();
    }

    private void manualRefresh() {
        updateReloadButtonVisibility(/* isReloading= */ true);
        if (mReliabilityLogger != null) {
            mReliabilityLogger
                    .getLaunchLogger()
                    .logManualRefresh(SystemClock.elapsedRealtimeNanos());
        }
        mMediator.manualRefresh(
                (Boolean v) -> {
                    updateReloadButtonVisibility(/* isReloading= */ false);
                    if (mSwipeRefreshLayout == null) return;
                    mSwipeRefreshLayout.setRefreshing(false);
                    mSwipeRefreshLayout.setAccessibilityLiveRegion(
                            View.ACCESSIBILITY_LIVE_REGION_NONE);
                    mSwipeRefreshLayout.setContentDescription("");
                });
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
    @Nullable FeedSurfaceLifecycleManager getSurfaceLifecycleManager() {
        return mFeedSurfaceLifecycleManager;
    }

    /**
     * @return whether this coordinator is currently active.
     */
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
    public void restoreInstanceState(@Nullable String state) {
        mMediator.restoreSavedInstanceState(state);
    }

    /**
     * Gets the appropriate {@link StreamTabId} for the given {@link NewTabPageLaunchOrigin}.
     *
     * <p>If coming from a Web Feed button, open the following tab, otherwise open the for you tab.
     */
    @VisibleForTesting
    @StreamTabId
    int getTabIdFromLaunchOrigin(@NewTabPageLaunchOrigin int launchOrigin) {
        return launchOrigin == NewTabPageLaunchOrigin.WEB_FEED
                ? StreamTabId.FOLLOWING
                : StreamTabId.DEFAULT;
    }

    @Initializer
    private RecyclerView setUpView() {
        mContentManager = new FeedListContentManager();
        ProcessScope processScope = FeedSurfaceTracker.getInstance().getXSurfaceProcessScope();
        if (processScope != null) {
            mDependencyProvider =
                    new FeedSurfaceScopeDependencyProviderImpl(
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
                    || CommandLine.getInstance()
                            .hasSwitch("force-enable-feed-reliability-logging")) {
                FeedLaunchReliabilityLogger launchLogger =
                        mSurfaceScope.getLaunchReliabilityLogger();
                FeedUserInteractionReliabilityLogger userInteractionLogger =
                        mSurfaceScope.getUserInteractionReliabilityLogger();
                FeedCardOpeningReliabilityLogger cardOpeningLogger =
                        mSurfaceScope.getCardOpeningReliabilityLogger();
                mReliabilityLogger =
                        new FeedReliabilityLogger(
                                launchLogger, userInteractionLogger, cardOpeningLogger);
                launchLogger.logUiStarting(
                        SurfaceType.NEW_TAB_PAGE, mEmbeddingSurfaceCreatedTimeNs);
            }

        } else {
            mHybridListRenderer = new NativeViewListRenderer(mActivity);
        }

        int gutterPadding = -1;
        if (mUseStaggeredLayout) {
            gutterPadding =
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(
                                    ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CONTAINMENT)
                                            ? R.dimen.feed_containment_gutter_padding_per_column
                                            : R.dimen.feed_gutter_padding_per_column);
        }
        // XSurface returns a View, but it should be a RecyclerView.
        RecyclerView view =
                (RecyclerView)
                        mHybridListRenderer.bind(mContentManager, mViewportView, gutterPadding);
        assumeNonNull(view);
        view.setId(R.id.feed_stream_recycler_view);
        view.setClipToPadding(false);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CONTAINMENT)) {
            // Used to draw containment background.
            view.addItemDecoration(
                    new FeedItemDecoration(
                            mActivity,
                            this,
                            (resId) -> {
                                return AppCompatResources.getDrawable(mActivity, resId);
                            },
                            gutterPadding));
        }

        // Work around https://crbug.com/943873 where default focus highlight shows up after
        // toggling dark mode.
        view.setDefaultFocusHighlightEnabled(false);
        if (mOverScrollDisabled) {
            view.setOverScrollMode(View.OVER_SCROLL_NEVER);
        }
        // Always add the TracingAndPerfScrollListener so debugging traces and metrics continue
        // to work.
        view.addOnScrollListener(new TracingAndPerfScrollListener());
        return view;
    }

    /** @return The {@link RecyclerView} associated with this feed. */
    public RecyclerView getRecyclerView() {
        return mRecyclerView;
    }

    /** @return The {@link FeedSurfaceScope} used to create this feed. */
    @Nullable FeedSurfaceScope getSurfaceScope() {
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
    public @Nullable FeedReliabilityLogger getReliabilityLogger() {
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
            mFeedSurfaceLifecycleManager =
                    mDelegate.createStreamLifecycleManager(mActivity, this, mProfile);
            headerList.add(mHeaderView);
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
        return new FeedStream(
                mActivity,
                mProfile,
                mSnackbarManager,
                mBottomSheetController,
                mWindowAndroid,
                mShareSupplier,
                kind,
                mActionDelegate,
                /* feedContentFirstLoadWatcher= */ this,
                streamsMediator,
                /* singleWebFeedParameters= */ null,
                new FeedSurfaceRendererBridge.Factory() {});
    }

    private void setHeaders(List<View> headerViews) {
        // Build the list of headers we want, and then replace existing headers.
        List<FeedListContentManager.FeedContent> headerList = new ArrayList<>();
        boolean hasSigninPromoView = false;
        for (View header : headerViews) {
            // Feed header view in multi does not need padding added.
            int lateralPaddingsPx = getLateralPaddingsPx();

            if (header instanceof NewTabPageLayout) {
                lateralPaddingsPx = 0;
            } else if (header == mHeaderView) {
                lateralPaddingsPx = 0;
                if (!ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CONTAINMENT)) {
                    mHeaderView.setBackgroundColor(mDefaultBackgroundColor);
                }
            } else if (header == mSigninPromoView) {
                hasSigninPromoView = true;
                lateralPaddingsPx =
                        mActivity
                                .getResources()
                                .getDimensionPixelSize(
                                        ChromeFeatureList.isEnabled(
                                                        ChromeFeatureList.FEED_CONTAINMENT)
                                                ? R.dimen
                                                        .feed_containment_signin_promo_lateral_paddings
                                                : R.dimen.signin_promo_lateral_paddings);
                ((PersonalizedSigninPromoView) mSigninPromoView)
                        .setCardBackgroundResource(
                                ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CONTAINMENT)
                                        ? R.drawable.home_surface_background_rounded
                                        : R.drawable.home_surface_ui_background);

                if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_HEADER_REMOVAL)) {
                    int topPadding =
                            mActivity
                                    .getResources()
                                    .getDimensionPixelSize(R.dimen.feed_header_top_margin);
                    mSigninPromoView.setPadding(
                            mSigninPromoView.getPaddingLeft(),
                            topPadding,
                            mSigninPromoView.getPaddingRight(),
                            mSigninPromoView.getPaddingBottom());
                }
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
        // The section header is the last header to be added, excluding sign-in promo, save its
        // index.
        mHeaderIndex = headerViews.size() - (hasSigninPromoView ? 2 : 1);
    }

    /**
     * @return The {@link SectionHeaderListProperties} model for the Feed section header.
     */
    @Nullable PropertyModel getSectionHeaderModelForTest() {
        return mSectionHeaderModel;
    }

    /**
     * @return The {@link View} for this class.
     */
    // TODO(crbug.com/352735671): Remove after uno phase 2 follow-up launch.
    @Deprecated
    View getSigninPromoView() {
        assert !ChromeFeatureList.isEnabled(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP);
        if (mSigninPromoView == null) {
            LayoutInflater inflater = LayoutInflater.from(mRootView.getContext());
            mSigninPromoView =
                    inflater.inflate(
                            R.layout.sync_promo_view_content_suggestions, mRootView, false);
        }
        return mSigninPromoView;
    }

    /** Update header views in the Feed. */
    void updateHeaderViews(@Nullable View signinPromoView) {
        if (!mMediator.hasStreams()) return;

        List<View> headers = new ArrayList<>();
        if (mNtpHeader != null) {
            headers.add(mNtpHeader);
        }

        if (mMediator.isFeedEnabled()) {
            headers.add(mHeaderView);

            if (signinPromoView != null) {
                mSigninPromoView = signinPromoView;
                headers.add(signinPromoView);
            }
        }

        setHeaders(headers);
    }

    void updateHeaderText(@Nullable String headerText) {
        if (headerText == null) {
            mHeaderView.setVisibility(View.GONE);
        } else {
            mHeaderView.setVisibility(View.VISIBLE);
            TextView titleView = (TextView) mHeaderView.findViewById(R.id.header_title);
            if (titleView != null) {
                titleView.setText(headerText);
            }
        }
    }

    public boolean isHeaderVisible() {
        return mHeaderView.getVisibility() == View.VISIBLE;
    }

    public FeedSurfaceMediator getMediatorForTesting() {
        return mMediator;
    }

    public void setMediatorForTesting(FeedSurfaceMediator mediator) {
        mMediator = mediator;
    }

    public View getHeaderViewForTesting() {
        return mHeaderView;
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
        // Don't do anything when there is no feed stream because the bubble isn't needed in
        // that case.
        if (!mMediator.hasStreams()) return;

        // Provide a delegate for the container of the feed surface that is handled by the feed
        // coordinator itself when not provided externally (e.g., by the NewTabPage).
        if (mScrollableContainerDelegate == null) {
            mScrollableContainerDelegate = new ScrollableContainerDelegateImpl();
        }

        if (!mIsNewTabPageCustomizationEnabled) {
            createHeaderIphScrollListener();
        }
        createRefreshIphScrollListener();
    }

    private void createHeaderIphScrollListener() {
        assumeNonNull(mScrollableContainerDelegate);
        mHeaderIphScrollListener =
                new HeaderIphScrollListener(
                        this,
                        mScrollableContainerDelegate,
                        () -> {
                            UserEducationHelper helper =
                                    new UserEducationHelper(mActivity, mProfile, mHandler);
                            ((SectionHeaderView) mHeaderView).showMenuIph(helper);
                        });
        mScrollableContainerDelegate.addScrollListener(mHeaderIphScrollListener);
    }

    private void createRefreshIphScrollListener() {
        assumeNonNull(mScrollableContainerDelegate);
        mRefreshIphScrollListener =
                new RefreshIphScrollListener(
                        this,
                        mScrollableContainerDelegate,
                        () -> {
                            UserEducationHelper helper =
                                    new UserEducationHelper(mActivity, mProfile, mHandler);
                            mSwipeRefreshLayout.showIph(helper);
                        });
        mScrollableContainerDelegate.addScrollListener(mRefreshIphScrollListener);
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
        }
        stopScrollTracking();
    }

    @Override
    public Tracker getFeatureEngagementTracker() {
        return TrackerFactory.getTrackerForProfile(mProfile);
    }

    @Override
    public boolean isFeedExpanded() {
        return mMediator.isSuggestionsVisible();
    }

    @Override
    public boolean isSignedIn() {
        return FeedServiceBridge.isSignedIn();
    }

    @Override
    public boolean isFeedHeaderPositionInContainerSuitableForIph(float headerMaxPosFraction) {
        assert headerMaxPosFraction >= 0.0f && headerMaxPosFraction <= 1.0f
                : "Max position fraction should be ranging between 0.0 and 1.0";

        assumeNonNull(mScrollableContainerDelegate);
        int topPosInStream =
                mScrollableContainerDelegate.getTopPositionRelativeToContainerView(mHeaderView);
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

    @Override
    public ObservableSupplier<Integer> getRestoringStateSupplier() {
        return mMediator.getRestoringStateSupplier();
    }

    @Override
    public List<String> getFeedUrls() {
        return mMediator.getFeedUrls();
    }

    private int getLateralPaddingsPx() {
        return mActivity
                .getResources()
                .getDimensionPixelSize(R.dimen.ntp_header_lateral_paddings_v2);
    }

    @Override
    public @ClosedReason int getClosedReason() {
        return mMediator.getClosedReason();
    }

    public void setReliabilityLoggerForTesting(FeedReliabilityLogger logger) {
        var oldValue = mReliabilityLogger;
        mReliabilityLogger = logger;
        ResettersForTesting.register(() -> mReliabilityLogger = oldValue);
    }

    public void clearScrollableContainerDelegateForTesting() {
        mScrollableContainerDelegate = null;
    }

    public FeedActionDelegate getActionDelegateForTesting() {
        return mActionDelegate;
    }

    FrameLayout getRootViewForTesting() {
        return mRootView;
    }

    public void setBackgroundImageCoordinatorForTesting(
            NtpBackgroundImageCoordinator backgroundImageCoordinator) {
        mNtpBackgroundImageCoordinator = backgroundImageCoordinator;
    }
}
