// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.MathUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.video_tutorials.NewTabPageVideoIPHManager;
import org.chromium.chrome.browser.compositor.layouts.content.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.cryptids.ProbabilisticCryptidRenderer;
import org.chromium.chrome.browser.explore_sites.ExperimentalExploreSitesSection;
import org.chromium.chrome.browser.explore_sites.ExploreSitesBridge;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.LogoBridge.Logo;
import org.chromium.chrome.browser.ntp.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.ntp.NewTabPage.OnSearchBoxScrollListener;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.query_tiles.QueryTileSection;
import org.chromium.chrome.browser.query_tiles.QueryTileUtils;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.tile.SiteSection;
import org.chromium.chrome.browser.suggestions.tile.SiteSectionViewHolder;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileGridLayout;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.suggestions.tile.TileRenderer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialServiceFactory;
import org.chromium.chrome.browser.video_tutorials.iph.VideoTutorialTryNowTracker;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.vr.VrModeObserver;

/**
 * Layout for the new tab page. This positions the page elements in the correct vertical positions.
 * There are no separate phone and tablet UIs; this layout adapts based on the available space.
 */
public class NewTabPageLayout extends LinearLayout implements TileGroup.Observer, VrModeObserver {
    private static final String TAG = "NewTabPageLayout";
    // Used to signify the cached resource value is unset.
    private static final int UNSET_RESOURCE_FLAG = -1;

    private final int mTileGridLayoutBleed;
    private int mSearchBoxEndPadding = UNSET_RESOURCE_FLAG;

    private View mMiddleSpacer; // Spacer between toolbar and Most Likely.

    private LogoView mSearchProviderLogoView;
    private SearchBoxCoordinator mSearchBoxCoordinator;
    private ViewGroup mSiteSectionView;
    private SiteSectionViewHolder mSiteSectionViewHolder;
    private View mTileGridPlaceholder;
    private View mNoSearchLogoSpacer;
    private QueryTileSection mQueryTileSection;
    private NewTabPageVideoIPHManager mVideoIPHManager;
    private ImageView mCryptidHolder;

    @Nullable
    private View mExploreSectionView; // View is null if explore flag is disabled.
    @Nullable
    private Object mExploreSection; // Null when explore sites disabled.

    private OnSearchBoxScrollListener mSearchBoxScrollListener;

    private NewTabPageManager mManager;
    private Activity mActivity;
    private LogoDelegateImpl mLogoDelegate;
    private TileGroup mTileGroup;
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

    private ScrollDelegate mScrollDelegate;

    private NewTabPageUma mNewTabPageUma;

    /**
     * A delegate used to obtain information about scroll state and perform various scroll
     * functions.
     */
    public interface ScrollDelegate {
        /**
         * @return Whether the scroll view is initialized. If false, the other delegate methods
         *         may not be valid.
         */
        boolean isScrollViewInitialized();

        /**
         * Checks whether the child at a given position is visible.
         * @param position The position of the child to check.
         * @return True if the child is at least partially visible.
         */
        boolean isChildVisibleAtPosition(int position);

        /**
         * @return The vertical scroll offset of the view containing this layout in pixels. Not
         *         valid until scroll view is initialized.
         */
        int getVerticalScrollOffset();

        /**
         * Snaps the scroll point of the scroll view to prevent the user from scrolling to midway
         * through a transition.
         */
        void snapScroll();
    }

    /**
     * Constructor for inflating from XML.
     */
    public NewTabPageLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        Resources res = getResources();
        mTileGridLayoutBleed = res.getDimensionPixelSize(R.dimen.tile_grid_layout_bleed);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMiddleSpacer = findViewById(R.id.ntp_middle_spacer);
        mSearchProviderLogoView = findViewById(R.id.search_provider_logo);
        mVideoIPHManager = new NewTabPageVideoIPHManager(
                findViewById(R.id.video_iph_stub), Profile.getLastUsedRegularProfile());
        insertSiteSectionView();

        int variation = ExploreSitesBridge.getVariation();
        if (ExploreSitesBridge.isExperimental(variation)) {
            ViewStub exploreStub = findViewById(R.id.explore_sites_stub);
            exploreStub.setLayoutResource(R.layout.experimental_explore_sites_section);
            mExploreSectionView = exploreStub.inflate();
        }
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
     * @param contextMenuManager The manager for long-press context menus.
     * @param uiConfig UiConfig that provides display information about this view.
     * @param tabProvider Provides the current active tab.
     * @param lifecycleDispatcher Activity lifecycle dispatcher.
     * @param uma {@link NewTabPageUma} object recording user metrics.
     * @param isIncognito Whether the new tab page is in incognito mode.
     * @param windowAndroid An instance of a {@link WindowAndroid}
     */
    public void initialize(NewTabPageManager manager, Activity activity,
            TileGroup.Delegate tileGroupDelegate, boolean searchProviderHasLogo,
            boolean searchProviderIsGoogle, ScrollDelegate scrollDelegate,
            ContextMenuManager contextMenuManager, UiConfig uiConfig, Supplier<Tab> tabProvider,
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
        OfflinePageBridge offlinePageBridge =
                SuggestionsDependencyFactory.getInstance().getOfflinePageBridge(profile);
        TileRenderer tileRenderer =
                new TileRenderer(mActivity, SuggestionsConfig.getTileStyle(mUiConfig),
                        getTileTitleLines(), mManager.getImageFetcher());
        mTileGroup = new TileGroup(tileRenderer, mManager, contextMenuManager, tileGroupDelegate,
                /* observer = */ this, offlinePageBridge);

        int maxRows = 2;
        if (searchProviderIsGoogle && QueryTileUtils.isQueryTilesEnabledOnNTP()) {
            maxRows = QueryTileSection.getMaxRowsForMostVisitedTiles(getContext());
        }

        mSiteSectionViewHolder =
                SiteSection.createViewHolder(getSiteSectionView(), mUiConfig, maxRows);
        mSiteSectionViewHolder.bindDataSource(mTileGroup, tileRenderer);

        int variation = ExploreSitesBridge.getVariation();
        if (ExploreSitesBridge.isExperimental(variation)) {
            mExploreSection = new ExperimentalExploreSitesSection(
                    mExploreSectionView, profile, mManager.getNavigationDelegate());
        }

        mSearchProviderLogoView = findViewById(R.id.search_provider_logo);
        mLogoDelegate = new LogoDelegateImpl(
                mManager.getNavigationDelegate(), mSearchProviderLogoView, profile);

        mSearchBoxCoordinator = new SearchBoxCoordinator(getContext(), this);
        mSearchBoxCoordinator.initialize(lifecycleDispatcher, mIsIncognito, mWindowAndroid);
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)) {
            mSearchBoxBoundsVerticalInset = getResources().getDimensionPixelSize(
                    R.dimen.ntp_search_box_bounds_vertical_inset_modern);
        }
        mNoSearchLogoSpacer = findViewById(R.id.no_search_logo_spacer);

        initializeSearchBoxTextView();
        initializeVoiceSearchButton();
        initializeLensButton();
        initializeLayoutChangeListener();
        setSearchProviderInfo(searchProviderHasLogo, searchProviderIsGoogle);
        mSearchProviderLogoView.showSearchProviderInitialView();

        if (searchProviderIsGoogle && QueryTileUtils.isQueryTilesEnabledOnNTP()) {
            mQueryTileSection = new QueryTileSection(findViewById(R.id.query_tiles),
                    mSearchBoxCoordinator, profile, mManager::performSearchQuery);
        }

        mTileGroup.startObserving(maxRows * getMaxColumnsForMostVisitedTiles());

        VrModuleProvider.registerVrModeObserver(this);
        if (VrModuleProvider.getDelegate().isInVr()) onEnterVr();

        manager.addDestructionObserver(NewTabPageLayout.this::onDestroy);
        mInitialized = true;

        TraceEvent.end(TAG + ".initialize()");
    }

    /**
     * @return The {@link ScrollDelegate} for this class.
     */
    ScrollDelegate getScrollDelegate() {
        return mScrollDelegate;
    }

    /**
     * Sets up the hint text and event handlers for the search box text view.
     */
    private void initializeSearchBoxTextView() {
        TraceEvent.begin(TAG + ".initializeSearchBoxTextView()");

        mSearchBoxCoordinator.setSearchBoxClickListener(
                v -> mManager.focusSearchBox(false, null, false));
        mSearchBoxCoordinator.setSearchBoxTextWatcher(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                if (s.length() == 0) return;
                mManager.focusSearchBox(
                        false, s.toString(), mSearchBoxCoordinator.isTextChangeFromTiles());
                mSearchBoxCoordinator.setSearchText("", false);
            }
        });
        TraceEvent.end(TAG + ".initializeSearchBoxTextView()");
    }

    private void initializeVoiceSearchButton() {
        TraceEvent.begin(TAG + ".initializeVoiceSearchButton()");
        mSearchBoxCoordinator.addVoiceSearchButtonClickListener(
                v -> mManager.focusSearchBox(true, null, false));
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
        mSiteSectionView = SiteSection.inflateSiteSection(this);
        ViewGroup.LayoutParams layoutParams = mSiteSectionView.getLayoutParams();
        layoutParams.width = ViewGroup.LayoutParams.WRAP_CONTENT;
        mSiteSectionView.setLayoutParams(layoutParams);

        int insertionPoint = indexOfChild(mMiddleSpacer) + 1;
        addView(mSiteSectionView, insertionPoint);
    }

    /**
     * @return the embedded {@link TileGridLayout}.
     */
    public ViewGroup getSiteSectionView() {
        return mSiteSectionView;
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

    /**
     * @return The placeholder that is shown above the fold when there is no other content to show,
     *         or null if it has not been inflated yet.
     */
    @VisibleForTesting
    @Nullable
    public View getPlaceholder() {
        return mTileGridPlaceholder;
    }

    @VisibleForTesting
    public TileGroup getTileGroup() {
        return mTileGroup;
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
        loadSearchProviderLogo();
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
     * Loads the search provider logo (e.g. Google doodle), if any.
     */
    public void loadSearchProviderLogo() {
        if (!mSearchProviderHasLogo) return;

        mSearchProviderLogoView.showSearchProviderInitialView();

        mLogoDelegate.getSearchProviderLogo(new LogoObserver() {
            @Override
            public void onLogoAvailable(Logo logo, boolean fromCache) {
                if (logo == null && fromCache) {
                    // There is no cached logo. Wait until we know whether there's a fresh
                    // one before making any further decisions.
                    return;
                }

                mSearchProviderLogoView.setDelegate(mLogoDelegate);
                mSearchProviderLogoView.updateLogo(logo);
                mSnapshotTileGridChanged = true;

                mShowingNonStandardLogo = logo != null;
                maybeKickOffCryptidRendering();
            }

            @Override
            public void onCachedLogoRevalidated() {
                maybeKickOffCryptidRendering();
            }
        });
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

        updateTileGridPadding();

        // Hide or show the views above the tile grid as needed, including logo, search box, and
        // spacers.
        mSearchProviderLogoView.setVisibility(shouldShowLogo() ? View.VISIBLE : View.GONE);
        mSearchBoxCoordinator.setVisibility(mSearchProviderHasLogo);
        updateTileGridPlaceholderVisibility();

        onUrlFocusAnimationChanged();

        mSnapshotTileGridChanged = true;
    }

    /**
     * Updates the padding for the tile grid based on what is shown above it.
     */
    private void updateTileGridPadding() {
        // Set a bit more top padding on the tile grid if there is no logo.
        int paddingTop = getResources().getDimensionPixelSize(shouldShowLogo()
                        ? R.dimen.tile_grid_layout_padding_top
                        : R.dimen.tile_grid_layout_no_logo_padding_top);
        mSiteSectionViewHolder.itemView.setPadding(
                0, paddingTop, 0, mSiteSectionViewHolder.itemView.getPaddingBottom());
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
        mSearchProviderLogoView.setAlpha(alpha);
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
        ((MarginLayoutParams) mSearchProviderLogoView.getLayoutParams()).topMargin = topMargin;
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
        mSearchProviderLogoView.endFadeAnimation();
        mSnapshotTileGridChanged = false;
    }

    /**
     * Shows the most visited placeholder ("Nothing to see here") if there are no most visited
     * items and there is no search provider logo.
     */
    private void updateTileGridPlaceholderVisibility() {
        boolean showPlaceholder =
                mTileGroup.hasReceivedData() && mTileGroup.isEmpty() && !mSearchProviderHasLogo;

        mNoSearchLogoSpacer.setVisibility(
                (mSearchProviderHasLogo || showPlaceholder) ? View.GONE : View.INVISIBLE);

        mSiteSectionViewHolder.itemView.setVisibility(showPlaceholder ? GONE : VISIBLE);

        if (showPlaceholder) {
            if (mTileGridPlaceholder == null) {
                ViewStub placeholderStub = findViewById(R.id.tile_grid_placeholder_stub);
                mTileGridPlaceholder = placeholderStub.inflate();
            }
            mTileGridPlaceholder.setVisibility(VISIBLE);
        } else if (mTileGridPlaceholder != null) {
            mTileGridPlaceholder.setVisibility(GONE);
        }
    }

    /**
     * Determines The maximum number of tiles to try and fit in a row. On smaller screens, there
     * may not be enough space to fit all of them.
     */
    private int getMaxColumnsForMostVisitedTiles() {
        return 4;
    }

    private static int getTileTitleLines() {
        return 1;
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

    // TileGroup.Observer interface.

    @Override
    public void onTileDataChanged() {
        mSiteSectionViewHolder.refreshData();
        mSnapshotTileGridChanged = true;

        // The page contents are initially hidden; otherwise they'll be drawn centered on the page
        // before the tiles are available and then jump upwards to make space once the tiles are
        // available.
        if (getVisibility() != View.VISIBLE) setVisibility(View.VISIBLE);
    }

    @Override
    public void onTileCountChanged() {
        // If the number of tile rows change while the URL bar is focused, the icons'
        // position will be wrong. Schedule the translation to be updated.
        if (mUrlFocusChangePercent == 1f) mTileCountChanged = true;
        updateTileGridPlaceholderVisibility();
    }

    @Override
    public void onTileIconChanged(Tile tile) {
        mSiteSectionViewHolder.updateIconView(tile);
        mSnapshotTileGridChanged = true;
    }

    @Override
    public void onTileOfflineBadgeVisibilityChanged(Tile tile) {
        mSiteSectionViewHolder.updateOfflineBadge(tile);
        mSnapshotTileGridChanged = true;
    }

    @Override
    public void onEnterVr() {
        mSearchBoxCoordinator.setVisibility(false);
    }

    @Override
    public void onExitVr() {
        mSearchBoxCoordinator.setVisibility(true);
    }

    private void onDestroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

        VrModuleProvider.unregisterVrModeObserver(this);

        if (mSearchProviderLogoView != null) {
            mSearchProviderLogoView.destroy();
        }

        if (mLogoDelegate != null) {
            mLogoDelegate.destroy();
        }

        mSearchBoxCoordinator.destroy();
    }

    private void maybeShowVideoTutorialTryNowIPH() {
        if (getToolbarTransitionPercentage() > 0f) return;
        VideoTutorialTryNowTracker tryNowTracker = VideoTutorialServiceFactory.getTryNowTracker();
        UserEducationHelper userEducationHelper = new UserEducationHelper(mActivity, new Handler());
        // TODO(shaktisahu): Determine if there is conflict with another IPH.
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

    private static IPHCommandBuilder createIPHCommandBuilder(Resources resources,
            @StringRes int stringId, @StringRes int accessibilityStringId, View anchorView,
            boolean showHighlight) {
        IPHCommandBuilder iphCommandBuilder =
                new IPHCommandBuilder(resources, null, stringId, accessibilityStringId);
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
        if (mSiteSectionView.getVisibility() != GONE) {
            final int width = mSiteSectionView.getMeasuredWidth() - mTileGridLayoutBleed;
            measureExactly(getSearchBoxView(), width, getSearchBoxView().getMeasuredHeight());
            measureExactly(
                    mSearchProviderLogoView, width, mSearchProviderLogoView.getMeasuredHeight());

            if (mExploreSectionView != null) {
                measureExactly(mExploreSectionView, mSiteSectionView.getMeasuredWidth(),
                        mExploreSectionView.getMeasuredHeight());
            }
        } else if (mExploreSectionView != null) {
            final int exploreWidth = mExploreSectionView.getMeasuredWidth() - mTileGridLayoutBleed;
            measureExactly(
                    getSearchBoxView(), exploreWidth, getSearchBoxView().getMeasuredHeight());
            measureExactly(mSearchProviderLogoView, exploreWidth,
                    mSearchProviderLogoView.getMeasuredHeight());
        }
    }

    /**
     * Convenience method to call measure() on the given View with MeasureSpecs converted from the
     * given dimensions (in pixels) with MeasureSpec.EXACTLY.
     */
    private static void measureExactly(View view, int widthPx, int heightPx) {
        view.measure(MeasureSpec.makeMeasureSpec(widthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY));
    }
}
