// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.MathUtils;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.video_tutorials.NewTabPageVideoIPHManager;
import org.chromium.chrome.browser.compositor.layouts.content.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.cryptids.ProbabilisticCryptidRenderer;
import org.chromium.chrome.browser.feed.FeedSurfaceScrollDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoCoordinator;
import org.chromium.chrome.browser.ntp.NewTabPage.OnSearchBoxScrollListener;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.query_tiles.QueryTileSection;
import org.chromium.chrome.browser.query_tiles.QueryTileUtils;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesCoordinator;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNTP;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialServiceFactory;
import org.chromium.chrome.browser.video_tutorials.iph.VideoTutorialTryNowTracker;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;

/**
 * Layout for the new tab page. This positions the page elements in the correct vertical positions.
 * There are no separate phone and tablet UIs; this layout adapts based on the available space.
 */
public class NewTabPageLayout extends LinearLayout {
    private static final String TAG = "NewTabPageLayout";

    // Used to signify the cached resource value is unset.
    private static final int UNSET_RESOURCE_FLAG = -1;

    private final int mTileGridLayoutBleed;
    private final Context mContext;
    private int mSearchBoxEndPadding = UNSET_RESOURCE_FLAG;

    private View mMiddleSpacer; // Spacer between toolbar and Most Likely.

    private LogoCoordinator mLogoCoordinator;
    private SearchBoxCoordinator mSearchBoxCoordinator;
    private QueryTileSection mQueryTileSection;
    private NewTabPageVideoIPHManager mVideoIPHManager;
    private ImageView mCryptidHolder;
    private ViewGroup mMvTilesContainerLayout;
    private MostVisitedTilesCoordinator mMostVisitedTilesCoordinator;

    private OnSearchBoxScrollListener mSearchBoxScrollListener;

    private NewTabPageManager mManager;
    private Activity mActivity;
    private UiConfig mUiConfig;
    private CallbackController mCallbackController = new CallbackController();

    /**
     * Whether the tiles shown in the layout have finished loading.
     * With {@link #mHasShownView}, it's one of the 2 flags used to track initialisation progress.
     */
    private boolean mTilesLoaded;

    /**
     * Whether the view has been shown at least once.
     * With {@link #mTilesLoaded}, it's one of the 2 flags used to track initialization progress.
     */
    private boolean mHasShownView;

    private boolean mSearchProviderHasLogo = true;
    private boolean mSearchProviderIsGoogle;
    private boolean mShowingNonStandardLogo;

    private boolean mInitialized;

    private float mUrlFocusChangePercent;
    private boolean mDisableUrlFocusChangeAnimations;
    private boolean mIsViewMoving;

    /** Flag used to request some layout changes after the next layout pass is completed. */
    private boolean mTileCountChanged;
    private boolean mSnapshotTileGridChanged;
    private boolean mIsIncognito;
    private WindowAndroid mWindowAndroid;

    /**
     * Vertical inset to add to the top and bottom of the search box bounds. May be 0 if no inset
     * should be applied. See {@link Rect#inset(int, int)}.
     */
    private int mSearchBoxBoundsVerticalInset;

    private FeedSurfaceScrollDelegate mScrollDelegate;

    private NewTabPageUma mNewTabPageUma;

    /**
     * Constructor for inflating from XML.
     */
    public NewTabPageLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        Resources res = getResources();
        mTileGridLayoutBleed = res.getDimensionPixelSize(R.dimen.tile_grid_layout_bleed);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMiddleSpacer = findViewById(R.id.ntp_middle_spacer);
        mVideoIPHManager = new NewTabPageVideoIPHManager(
                findViewById(R.id.video_iph_stub), Profile.getLastUsedRegularProfile());
        insertSiteSectionView();
    }

    /**
     * Initializes the NewTabPageLayout. This must be called immediately after inflation, before
     * this object is used in any other way.
     * @param manager NewTabPageManager used to perform various actions when the user interacts
     *                with the page.
     * @param activity The activity that currently owns the new tab page
     * @param tileGroupDelegate Delegate for {@link TileGroup}.
     * @param searchProviderHasLogo Whether the search provider has a logo.
     * @param searchProviderIsGoogle Whether the search provider is Google.
     * @param scrollDelegate The delegate used to obtain information about scroll state.
     * @param touchEnabledDelegate The {@link TouchEnabledDelegate} for handling whether touch
     *         events are allowed.
     * @param uiConfig UiConfig that provides display information about this view.
     * @param lifecycleDispatcher Activity lifecycle dispatcher.
     * @param uma {@link NewTabPageUma} object recording user metrics.
     * @param isIncognito Whether the new tab page is in incognito mode.
     * @param windowAndroid An instance of a {@link WindowAndroid}
     */
    public void initialize(NewTabPageManager manager, Activity activity,
            TileGroup.Delegate tileGroupDelegate, boolean searchProviderHasLogo,
            boolean searchProviderIsGoogle, FeedSurfaceScrollDelegate scrollDelegate,
            TouchEnabledDelegate touchEnabledDelegate, UiConfig uiConfig,
            ActivityLifecycleDispatcher lifecycleDispatcher, NewTabPageUma uma, boolean isIncognito,
            WindowAndroid windowAndroid) {
        TraceEvent.begin(TAG + ".initialize()");
        mScrollDelegate = scrollDelegate;
        mManager = manager;
        mActivity = activity;
        mUiConfig = uiConfig;
        mNewTabPageUma = uma;
        mIsIncognito = isIncognito;
        mWindowAndroid = windowAndroid;
        Profile profile = Profile.getLastUsedRegularProfile();

        mSearchBoxCoordinator = new SearchBoxCoordinator(getContext(), this);
        mSearchBoxCoordinator.initialize(lifecycleDispatcher, mIsIncognito, mWindowAndroid);
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)) {
            mSearchBoxBoundsVerticalInset = getResources().getDimensionPixelSize(
                    R.dimen.ntp_search_box_bounds_vertical_inset_modern);
        }

        initializeLogoCoordinator(searchProviderHasLogo, searchProviderIsGoogle);
        initializeMostVisitedTilesCoordinator(profile, lifecycleDispatcher, tileGroupDelegate,
                touchEnabledDelegate, isScrollableMvtEnabled(), searchProviderIsGoogle);
        initializeSearchBoxBackground();
        initializeSearchBoxTextView();
        initializeVoiceSearchButton();
        initializeLensButton();
        initializeLayoutChangeListener();

        if (searchProviderIsGoogle && QueryTileUtils.isQueryTilesEnabledOnNTP()) {
            mQueryTileSection = new QueryTileSection(
                    findViewById(R.id.query_tiles), profile, mManager::performSearchQuery);
        }

        manager.addDestructionObserver(NewTabPageLayout.this::onDestroy);
        mInitialized = true;

        TraceEvent.end(TAG + ".initialize()");
    }

    /**
     * @return The {@link FeedSurfaceScrollDelegate} for this class.
     */
    FeedSurfaceScrollDelegate getScrollDelegate() {
        return mScrollDelegate;
    }

    /**
     * Sets up the search box background tint.
     */
    private void initializeSearchBoxBackground() {
        final int elevationDimenId = ChromeFeatureList.sBaselineGm3SurfaceColors.isEnabled()
                ? R.dimen.default_elevation_4
                : R.dimen.toolbar_text_box_elevation;
        final int searchBoxColor = ChromeColors.getSurfaceColor(getContext(), elevationDimenId);
        final ColorStateList colorStateList = ColorStateList.valueOf(searchBoxColor);
        findViewById(R.id.search_box).setBackgroundTintList(colorStateList);
    }

    /**
     * Sets up the hint text and event handlers for the search box text view.
     */
    private void initializeSearchBoxTextView() {
        TraceEvent.begin(TAG + ".initializeSearchBoxTextView()");

        mSearchBoxCoordinator.setSearchBoxClickListener(v -> mManager.focusSearchBox(false, null));
        mSearchBoxCoordinator.setSearchBoxTextWatcher(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                if (s.length() == 0) return;
                mManager.focusSearchBox(false, s.toString());
                mSearchBoxCoordinator.setSearchText("");
            }
        });
        TraceEvent.end(TAG + ".initializeSearchBoxTextView()");
    }

    private void initializeVoiceSearchButton() {
        TraceEvent.begin(TAG + ".initializeVoiceSearchButton()");
        mSearchBoxCoordinator.addVoiceSearchButtonClickListener(
                v -> mManager.focusSearchBox(true, null));
        updateActionButtonVisibility();
        TraceEvent.end(TAG + ".initializeVoiceSearchButton()");
    }

    private void initializeLensButton() {
        TraceEvent.begin(TAG + ".initializeLensButton()");
        // TODO(b/181067692): Report user action for this click.
        mSearchBoxCoordinator.addLensButtonClickListener(v -> {
            LensMetrics.recordClicked(LensEntryPoint.NEW_TAB_PAGE);
            mSearchBoxCoordinator.startLens(LensEntryPoint.NEW_TAB_PAGE);
        });
        updateActionButtonVisibility();
        TraceEvent.end(TAG + ".initializeLensButton()");
    }

    private void initializeLayoutChangeListener() {
        TraceEvent.begin(TAG + ".initializeLayoutChangeListener()");
        addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    int oldHeight = oldBottom - oldTop;
                    int newHeight = bottom - top;

                    if (oldHeight == newHeight && !mTileCountChanged) return;
                    mTileCountChanged = false;

                    // Re-apply the url focus change amount after a rotation to ensure the views are
                    // correctly placed with their new layout configurations.
                    onUrlFocusAnimationChanged();
                    updateSearchBoxOnScroll();

                    // The positioning of elements may have been changed (since the elements expand
                    // to fill the available vertical space), so adjust the scroll.
                    mScrollDelegate.snapScroll();
                });
        TraceEvent.end(TAG + ".initializeLayoutChangeListener()");
    }

    private void initializeLogoCoordinator(
            boolean searchProviderHasLogo, boolean searchProviderIsGoogle) {
        Callback<LoadUrlParams> logoClickedCallback =
                mCallbackController.makeCancelable((urlParams) -> {
                    mManager.getNativePageHost().loadUrl(urlParams, /*isIncognito=*/false);
                    BrowserUiUtils.recordModuleClickHistogram(
                            HostSurface.NEW_TAB_PAGE, ModuleTypeOnStartAndNTP.DOODLE);
                });
        Callback<Logo> onLogoAvailableCallback = mCallbackController.makeCancelable((logo) -> {
            mSnapshotTileGridChanged = true;
            mShowingNonStandardLogo = logo != null;
            maybeKickOffCryptidRendering();
        });
        Runnable onCachedLogoRevalidatedRunnable =
                mCallbackController.makeCancelable(this::maybeKickOffCryptidRendering);

        // If pull up Feed position is enabled, doodle is not supported since there is not enough
        // room, we don't need to fetch logo image.
        boolean shouldFetchDoodle = !FeedPositionUtils.isFeedPullUpEnabled();
        mLogoCoordinator = new LogoCoordinator(mContext, logoClickedCallback,
                findViewById(R.id.search_provider_logo), shouldFetchDoodle, onLogoAvailableCallback,
                onCachedLogoRevalidatedRunnable, /*isParentSurfaceShown=*/true,
                /*visibilityObserver=*/null);
        mLogoCoordinator.initWithNative();
        setSearchProviderInfo(searchProviderHasLogo, searchProviderIsGoogle);
    }

    private void initializeMostVisitedTilesCoordinator(Profile profile,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            TileGroup.Delegate tileGroupDelegate, TouchEnabledDelegate touchEnabledDelegate,
            boolean isScrollableMvtEnabled, boolean searchProviderIsGoogle) {
        assert mMvTilesContainerLayout != null;

        int maxRows = 2;
        if (searchProviderIsGoogle && QueryTileUtils.isQueryTilesEnabledOnNTP()) {
            maxRows = QueryTileSection.getMaxRowsForMostVisitedTiles(getContext());
        }

        mMostVisitedTilesCoordinator = new MostVisitedTilesCoordinator(mActivity,
                activityLifecycleDispatcher, mMvTilesContainerLayout, mWindowAndroid,
                /*shouldShowSkeletonUIPreNative=*/false, isScrollableMvtEnabled, maxRows,
                () -> mSnapshotTileGridChanged = true, () -> {
                    if (mUrlFocusChangePercent == 1f) mTileCountChanged = true;
                });

        mMostVisitedTilesCoordinator.initWithNative(
                mManager, tileGroupDelegate, touchEnabledDelegate);
    }

    /**
     * Updates the search box when the parent view's scroll position is changed.
     */
    void updateSearchBoxOnScroll() {
        if (mDisableUrlFocusChangeAnimations || mIsViewMoving) return;

        // When the page changes (tab switching or new page loading), it is possible that events
        // (e.g. delayed view change notifications) trigger calls to these methods after
        // the current page changes. We check it again to make sure we don't attempt to update the
        // wrong page.
        if (!mManager.isCurrentPage()) return;

        if (mSearchBoxScrollListener != null) {
            mSearchBoxScrollListener.onNtpScrollChanged(getToolbarTransitionPercentage());
        }
    }

    /**
     * Calculates the percentage (between 0 and 1) of the transition from the search box to the
     * omnibox at the top of the New Tab Page, which is determined by the amount of scrolling and
     * the position of the search box.
     *
     * @return the transition percentage
     */
    float getToolbarTransitionPercentage() {
        // During startup the view may not be fully initialized.
        if (!mScrollDelegate.isScrollViewInitialized()) return 0f;

        if (isSearchBoxOffscreen()) {
            // getVerticalScrollOffset is valid only for the scroll view if the first item is
            // visible. If the search box view is offscreen, we must have scrolled quite far and we
            // know the toolbar transition should be 100%. This might be the initial scroll position
            // due to the scroll restore feature, so the search box will not have been laid out yet.
            return 1f;
        }

        // During startup the view may not be fully initialized, so we only calculate the current
        // percentage if some basic view properties (position of the search box) are sane.
        int searchBoxTop = getSearchBoxView().getTop();
        if (searchBoxTop == 0) return 0f;

        // For all other calculations, add the search box padding, because it defines where the
        // visible "border" of the search box is.
        searchBoxTop += getSearchBoxView().getPaddingTop();

        final int scrollY = mScrollDelegate.getVerticalScrollOffset();
        // Use int pixel size instead of float dimension to avoid precision error on the percentage.
        final float transitionLength =
                getResources().getDimensionPixelSize(R.dimen.ntp_search_box_transition_length);
        // Tab strip height is zero on phones, nonzero on tablets.
        int tabStripHeight = getResources().getDimensionPixelSize(R.dimen.tab_strip_height);

        // |scrollY - searchBoxTop + tabStripHeight| gives the distance the search bar is from the
        // top of the tab.
        return MathUtils.clamp(
                (scrollY - searchBoxTop + tabStripHeight + transitionLength) / transitionLength, 0f,
                1f);
    }

    private void insertSiteSectionView() {
        int insertionPoint = indexOfChild(mMiddleSpacer) + 1;

        mMvTilesContainerLayout = (ViewGroup) LayoutInflater.from(this.getContext())
                                          .inflate(R.layout.mv_tiles_container, this, false);
        mMvTilesContainerLayout.setVisibility(View.VISIBLE);
        addView(mMvTilesContainerLayout, insertionPoint);
        // The page contents are initially hidden; otherwise they'll be drawn centered on the
        // page before the tiles are available and then jump upwards to make space once the
        // tiles are available.
        if (getVisibility() != View.VISIBLE) setVisibility(View.VISIBLE);
    }

    /**
     * @return The fake search box view.
     */
    public View getSearchBoxView() {
        return mSearchBoxCoordinator.getView();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        unifyElementWidths();
    }

    public void onSwitchToForeground() {
        if (mMostVisitedTilesCoordinator != null) {
            mMostVisitedTilesCoordinator.onSwitchToForeground();
        }
    }

    /**
     * Should be called every time one of the flags used to track initialization progress changes.
     * Finalizes initialization once all the preliminary steps are complete.
     *
     * @see #mHasShownView
     * @see #mTilesLoaded
     */
    private void onInitializationProgressChanged() {
        if (!hasLoadCompleted()) return;

        mManager.onLoadingComplete();

        // Load the logo after everything else is finished, since it's lower priority.
        mLogoCoordinator.loadSearchProviderLogoWithAnimation();
    }

    /**
     * To be called to notify that the tiles have finished loading. Will do nothing if a load was
     * previously completed.
     */
    public void onTilesLoaded() {
        if (mTilesLoaded) return;
        mTilesLoaded = true;

        onInitializationProgressChanged();
    }

    /**
     * Changes the layout depending on whether the selected search provider (e.g. Google, Bing)
     * has a logo.
     * @param hasLogo Whether the search provider has a logo.
     * @param isGoogle Whether the search provider is Google.
     */
    public void setSearchProviderInfo(boolean hasLogo, boolean isGoogle) {
        if (hasLogo == mSearchProviderHasLogo && isGoogle == mSearchProviderIsGoogle
                && mInitialized) {
            return;
        }
        mSearchProviderHasLogo = hasLogo;
        mSearchProviderIsGoogle = isGoogle;

        updateTilesLayoutMargins();

        // Hide or show the views above the tile grid as needed, including search box, and
        // spacers. The visibility of Logo is handled by LogoCoordinator.
        mSearchBoxCoordinator.setVisibility(mSearchProviderHasLogo);

        onUrlFocusAnimationChanged();

        mSnapshotTileGridChanged = true;
    }

    /**
     * Updates the margins for the tile grid based on what is shown above it.
     */
    private void updateTilesLayoutMargins() {
        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) mMvTilesContainerLayout.getLayoutParams();

        if (isScrollableMvtEnabled()) {
            // Let mMvTilesContainerLayout attached to the edge of the screen.
            setClipToPadding(false);
            int lateralPaddingsForNTP = mActivity.getResources().getDimensionPixelSize(
                    R.dimen.ntp_header_lateral_paddings_v2);
            marginLayoutParams.leftMargin = -lateralPaddingsForNTP;
            marginLayoutParams.rightMargin = -lateralPaddingsForNTP;
            marginLayoutParams.topMargin = getResources().getDimensionPixelSize(shouldShowLogo()
                            ? R.dimen.tile_grid_layout_top_margin
                            : R.dimen.tile_grid_layout_no_logo_top_margin);
            marginLayoutParams.bottomMargin = getResources().getDimensionPixelOffset(
                    R.dimen.tile_carousel_layout_bottom_margin);
        } else {
            // Set a bit more top padding on the tile grid if there is no logo.
            ViewGroup.LayoutParams layoutParams = mMvTilesContainerLayout.getLayoutParams();
            layoutParams.width = ViewGroup.LayoutParams.WRAP_CONTENT;
            marginLayoutParams.topMargin = getGridMvtTopMargin();
            marginLayoutParams.bottomMargin = getGridMvtBottomMargin();
        }
    }

    /**
     * Updates whether the NewTabPage should animate on URL focus changes.
     * @param disable Whether to disable the animations.
     */
    void setUrlFocusAnimationsDisabled(boolean disable) {
        if (disable == mDisableUrlFocusChangeAnimations) return;
        mDisableUrlFocusChangeAnimations = disable;
        if (!disable) onUrlFocusAnimationChanged();
    }

    /**
     * @return Whether URL focus animations are currently disabled.
     */
    boolean urlFocusAnimationsDisabled() {
        return mDisableUrlFocusChangeAnimations;
    }

    /**
     * Specifies the percentage the URL is focused during an animation.  1.0 specifies that the URL
     * bar has focus and has completed the focus animation.  0 is when the URL bar is does not have
     * any focus.
     *
     * @param percent The percentage of the URL bar focus animation.
     */
    void setUrlFocusChangeAnimationPercent(float percent) {
        mUrlFocusChangePercent = percent;
        onUrlFocusAnimationChanged();
    }

    /**
     * @return The percentage that the URL bar is focused during an animation.
     */
    @VisibleForTesting
    float getUrlFocusChangeAnimationPercent() {
        return mUrlFocusChangePercent;
    }

    void onUrlFocusAnimationChanged() {
        if (mDisableUrlFocusChangeAnimations || mIsViewMoving) return;

        // Translate so that the search box is at the top, but only upwards.
        float percent = mSearchProviderHasLogo ? mUrlFocusChangePercent : 0;
        int basePosition = mScrollDelegate.getVerticalScrollOffset() + getPaddingTop();
        int target = Math.max(basePosition,
                getSearchBoxView().getBottom() - getSearchBoxView().getPaddingBottom()
                        - mSearchBoxBoundsVerticalInset);

        setTranslationY(percent * (basePosition - target));
        if (mQueryTileSection != null) mQueryTileSection.onUrlFocusAnimationChanged(percent);
    }

    void onLoadUrl(boolean isNtpUrl) {
        if (isNtpUrl && mQueryTileSection != null) mQueryTileSection.reloadTiles();
    }

    /**
     * Sets whether this view is currently moving within its parent view. When the view is moving
     * certain animations will be disabled or prevented.
     * @param isViewMoving Whether this view is currently moving.
     */
    void setIsViewMoving(boolean isViewMoving) {
        mIsViewMoving = isViewMoving;
    }

    /**
     * Updates the opacity of the search box when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchBoxAlpha(float alpha) {
        mSearchBoxCoordinator.setAlpha(alpha);
    }

    /**
     * Updates the opacity of the search provider logo when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchProviderLogoAlpha(float alpha) {
        mLogoCoordinator.setAlpha(alpha);
    }

    /**
     * Set the search box background drawable.
     *
     * @param drawable The search box background.
     */
    public void setSearchBoxBackground(Drawable drawable) {
        mSearchBoxCoordinator.setBackground(drawable);
    }

    /**
     * Get the bounds of the search box in relation to the top level {@code parentView}.
     *
     * @param bounds The current drawing location of the search box.
     * @param translation The translation applied to the search box by the parent view hierarchy up
     *                    to the {@code parentView}.
     * @param parentView The top level parent view used to translate search box bounds.
     */
    void getSearchBoxBounds(Rect bounds, Point translation, View parentView) {
        int searchBoxX = (int) getSearchBoxView().getX();
        int searchBoxY = (int) getSearchBoxView().getY();
        bounds.set(searchBoxX, searchBoxY, searchBoxX + getSearchBoxView().getWidth(),
                searchBoxY + getSearchBoxView().getHeight());

        translation.set(0, 0);

        if (isSearchBoxOffscreen()) {
            translation.y = Integer.MIN_VALUE;
        } else {
            View view = getSearchBoxView();
            while (true) {
                view = (View) view.getParent();
                if (view == null) {
                    // The |mSearchBoxView| is not a child of this view. This can happen if the
                    // RecyclerView detaches the NewTabPageLayout after it has been scrolled out of
                    // view. Set the translation to the minimum Y value as an approximation.
                    translation.y = Integer.MIN_VALUE;
                    break;
                }
                translation.offset(-view.getScrollX(), -view.getScrollY());
                if (view == parentView) break;
                translation.offset((int) view.getX(), (int) view.getY());
            }
        }

        bounds.offset(translation.x, translation.y);
        if (translation.y != Integer.MIN_VALUE) {
            bounds.inset(0, mSearchBoxBoundsVerticalInset);
        }
    }

    void setSearchProviderTopMargin(int topMargin) {
        mLogoCoordinator.setTopMargin(topMargin);
    }

    void setSearchProviderBottomMargin(int bottomMargin) {
        mLogoCoordinator.setBottomMargin(bottomMargin);
    }

    /**
     * @return Whether the search box view is scrolled off the screen.
     */
    private boolean isSearchBoxOffscreen() {
        return !mScrollDelegate.isChildVisibleAtPosition(0)
                || mScrollDelegate.getVerticalScrollOffset() > getSearchBoxView().getTop();
    }

    /**
     * Sets the listener for search box scroll changes.
     * @param listener The listener to be notified on changes.
     */
    void setSearchBoxScrollListener(OnSearchBoxScrollListener listener) {
        mSearchBoxScrollListener = listener;
        if (mSearchBoxScrollListener != null) updateSearchBoxOnScroll();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        assert mManager != null;

        if (!mHasShownView) {
            mHasShownView = true;
            onInitializationProgressChanged();
            mNewTabPageUma.recordSearchAvailableLoadTime();
            TraceEvent.instant("NewTabPageSearchAvailable)");
        }
    }

    /** Update the visibility of the action buttons. */
    void updateActionButtonVisibility() {
        mSearchBoxCoordinator.setVoiceSearchButtonVisibility(mManager.isVoiceSearchEnabled());
        boolean shouldShowLensButton =
                mSearchBoxCoordinator.isLensEnabled(LensEntryPoint.NEW_TAB_PAGE);
        LensMetrics.recordShown(LensEntryPoint.NEW_TAB_PAGE, shouldShowLensButton);
        mSearchBoxCoordinator.setLensButtonVisibility(shouldShowLensButton);
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);

        // On first run, the NewTabPageLayout is initialized behind the First Run Experience,
        // meaning the UiConfig will pickup the screen layout then. However onConfigurationChanged
        // is not called on orientation changes until the FRE is completed. This means that if a
        // user starts the FRE in one orientation, changes an orientation and then leaves the FRE
        // the UiConfig will have the wrong orientation. https://crbug.com/683886.
        mUiConfig.updateDisplayStyle();

        if (visibility == VISIBLE) {
            updateActionButtonVisibility();
            maybeShowVideoTutorialTryNowIPH();
        }
    }

    /**
     * @see InvalidationAwareThumbnailProvider#shouldCaptureThumbnail()
     */
    public boolean shouldCaptureThumbnail() {
        return mSnapshotTileGridChanged;
    }

    /**
     * Should be called before a thumbnail of the parent view is captured.
     * @see InvalidationAwareThumbnailProvider#captureThumbnail(Canvas)
     */
    public void onPreCaptureThumbnail() {
        mLogoCoordinator.endFadeAnimation();
        mSnapshotTileGridChanged = false;
    }

    private boolean shouldShowLogo() {
        return mSearchProviderHasLogo;
    }

    private boolean hasLoadCompleted() {
        return mHasShownView && mTilesLoaded;
    }

    private void maybeKickOffCryptidRendering() {
        if (!mSearchProviderIsGoogle || mShowingNonStandardLogo) {
            // Cryptid rendering is disabled when the logo is not the standard Google logo.
            return;
        }

        ProbabilisticCryptidRenderer renderer = ProbabilisticCryptidRenderer.getInstance();
        renderer.getCryptidForLogo(Profile.getLastUsedRegularProfile(),
                mCallbackController.makeCancelable((drawable) -> {
                    if (drawable == null || mCryptidHolder != null) {
                        return;
                    }
                    ViewStub stub =
                            findViewById(R.id.logo_holder).findViewById(R.id.cryptid_holder);
                    ImageView view = (ImageView) stub.inflate();
                    view.setImageDrawable(drawable);
                    mCryptidHolder = view;
                    renderer.recordRenderEvent();
                }));
    }

    private void onDestroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

        if (mLogoCoordinator != null) {
            mLogoCoordinator.destroy();
            mLogoCoordinator = null;
        }

        mSearchBoxCoordinator.destroy();

        if (mMostVisitedTilesCoordinator != null) {
            mMostVisitedTilesCoordinator.destroyMvtiles();
            mMostVisitedTilesCoordinator = null;
        }
    }

    private void maybeShowVideoTutorialTryNowIPH() {
        if (getToolbarTransitionPercentage() > 0f) return;
        VideoTutorialTryNowTracker tryNowTracker = VideoTutorialServiceFactory.getTryNowTracker();
        UserEducationHelper userEducationHelper = new UserEducationHelper(mActivity, new Handler());
        if (tryNowTracker.didClickTryNowButton(FeatureType.SEARCH)) {
            IPHCommandBuilder iphCommandBuilder = createIPHCommandBuilder(mActivity.getResources(),
                    R.string.video_tutorials_iph_tap_here_to_start,
                    R.string.video_tutorials_iph_tap_here_to_start, mSearchBoxCoordinator.getView(),
                    false);
            userEducationHelper.requestShowIPH(iphCommandBuilder.build());
            tryNowTracker.tryNowUIShown(FeatureType.SEARCH);
        }

        if (tryNowTracker.didClickTryNowButton(FeatureType.VOICE_SEARCH)) {
            IPHCommandBuilder iphCommandBuilder = createIPHCommandBuilder(mActivity.getResources(),
                    R.string.video_tutorials_iph_tap_voice_icon_to_start,
                    R.string.video_tutorials_iph_tap_voice_icon_to_start,
                    mSearchBoxCoordinator.getVoiceSearchButton(), true);
            userEducationHelper.requestShowIPH(iphCommandBuilder.build());
            tryNowTracker.tryNowUIShown(FeatureType.VOICE_SEARCH);
        }
    }

    @VisibleForTesting
    MostVisitedTilesCoordinator getMostVisitedTilesCoordinatorForTesting() {
        return mMostVisitedTilesCoordinator;
    }

    void maybeShowFeatureNotificationVoiceSearchIPH() {
        IPHCommandBuilder iphCommandBuilder = createIPHCommandBuilder(mActivity.getResources(),
                R.string.feature_notification_guide_tooltip_message_voice_search,
                R.string.feature_notification_guide_tooltip_message_voice_search,
                mSearchBoxCoordinator.getVoiceSearchButton(), true);
        UserEducationHelper userEducationHelper = new UserEducationHelper(mActivity, new Handler());
        userEducationHelper.requestShowIPH(iphCommandBuilder.build());
    }

    private static IPHCommandBuilder createIPHCommandBuilder(Resources resources,
            @StringRes int stringId, @StringRes int accessibilityStringId, View anchorView,
            boolean showHighlight) {
        IPHCommandBuilder iphCommandBuilder = new IPHCommandBuilder(resources,
                FeatureConstants.FEATURE_NOTIFICATION_GUIDE_VOICE_SEARCH_HELP_BUBBLE_FEATURE,
                stringId, accessibilityStringId);
        iphCommandBuilder.setAnchorView(anchorView);
        int yInsetPx = resources.getDimensionPixelOffset(
                R.dimen.video_tutorial_try_now_iph_ntp_searchbox_y_inset);
        iphCommandBuilder.setInsetRect(new Rect(0, 0, 0, -yInsetPx));
        if (showHighlight) {
            iphCommandBuilder.setOnShowCallback(
                    ()
                            -> ViewHighlighter.turnOnHighlight(
                                    anchorView, new HighlightParams(HighlightShape.CIRCLE)));
            iphCommandBuilder.setOnDismissCallback(() -> new Handler().postDelayed(() -> {
                ViewHighlighter.turnOffHighlight(anchorView);
            }, ViewHighlighter.IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS));
        }

        return iphCommandBuilder;
    }

    /**
     * Makes the Search Box and Logo as wide as Most Visited.
     */
    private void unifyElementWidths() {
        View searchBoxView = getSearchBoxView();
        if (mMvTilesContainerLayout.getVisibility() != GONE) {
            if (!isScrollableMvtEnabled()) {
                final int width = mMvTilesContainerLayout.getMeasuredWidth() - mTileGridLayoutBleed;
                measureExactly(searchBoxView, width, searchBoxView.getMeasuredHeight());
                mLogoCoordinator.measureExactlyLogoView(width);
            } else {
                final int width = getMeasuredWidth() - mTileGridLayoutBleed;
                measureExactly(searchBoxView, width, searchBoxView.getMeasuredHeight());
                mLogoCoordinator.measureExactlyLogoView(width);
            }
        }
    }

    private boolean isScrollableMvtEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID)
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
    }

    // TODO(crbug.com/1329288): Remove this method when the Feed position experiment is cleaned up.
    private int getGridMvtTopMargin() {
        if (!shouldShowLogo()) {
            return getResources().getDimensionPixelOffset(
                    R.dimen.tile_grid_layout_no_logo_top_margin);
        }

        int resourcesId = R.dimen.tile_grid_layout_top_margin;

        if (FeedPositionUtils.isFeedPushDownLargeEnabled()) {
            resourcesId = R.dimen.tile_grid_layout_top_margin_push_down_large;
        } else if (FeedPositionUtils.isFeedPushDownSmallEnabled()) {
            resourcesId = R.dimen.tile_grid_layout_top_margin_push_down_small;
        } else if (FeedPositionUtils.isFeedPullUpEnabled()) {
            resourcesId = R.dimen.tile_grid_layout_top_margin_pull_up;
        }

        return getResources().getDimensionPixelOffset(resourcesId);
    }

    // TODO(crbug.com/1329288): Remove this method when the Feed position experiment is cleaned up.
    private int getGridMvtBottomMargin() {
        int resourcesId = R.dimen.tile_grid_layout_bottom_margin;

        if (!shouldShowLogo()) return getResources().getDimensionPixelOffset(resourcesId);

        if (FeedPositionUtils.isFeedPushDownLargeEnabled()) {
            resourcesId = R.dimen.tile_grid_layout_bottom_margin_push_down_large;
        } else if (FeedPositionUtils.isFeedPushDownSmallEnabled()) {
            resourcesId = R.dimen.tile_grid_layout_bottom_margin_push_down_small;
        } else if (FeedPositionUtils.isFeedPullUpEnabled()) {
            resourcesId = R.dimen.tile_grid_layout_bottom_margin_pull_up;
        }

        return getResources().getDimensionPixelOffset(resourcesId);
    }

    /**
     * Convenience method to call measure() on the given View with MeasureSpecs converted from the
     * given dimensions (in pixels) with MeasureSpec.EXACTLY.
     */
    private static void measureExactly(View view, int widthPx, int heightPx) {
        view.measure(MeasureSpec.makeMeasureSpec(widthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY));
    }

    @VisibleForTesting
    LogoCoordinator getLogoCoordinatorForTesting() {
        return mLogoCoordinator;
    }
}
