// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.content.res.Resources;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MemoryPressureListener;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.shared.FeedFeatures;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ScrollListener;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.cards.promo.enhanced_protection.EnhancedProtectionPromoController.EnhancedProtectionPromoStateListener;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.SigninPromoUtil;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.ui.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ui.SigninPromoController;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.Locale;

/**
 * A mediator for the {@link FeedSurfaceCoordinator} responsible for interacting with the
 * native library and handling business logic.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public class FeedSurfaceMediator
        implements NewTabPageLayout.ScrollDelegate, ContextMenuManager.TouchEnabledDelegate,
                   TemplateUrlServiceObserver, ListMenu.Delegate,
                   EnhancedProtectionPromoStateListener, IdentityManager.Observer {
    @VisibleForTesting
    public static final String FEED_CONTENT_FIRST_LOADED_TIME_MS_UMA = "FeedContentFirstLoadedTime";

    private static final float IPH_TRIGGER_BAR_TRANSITION_FRACTION = 1.0f;
    private static final float IPH_STREAM_MIN_SCROLL_FRACTION = 0.10f;
    private static final float IPH_FEED_HEADER_MAX_POS_FRACTION = 0.35f;

    private final FeedSurfaceCoordinator mCoordinator;
    private final @Nullable SnapScrollHelper mSnapScrollHelper;
    private final PrefChangeRegistrar mPrefChangeRegistrar;
    private final SigninManager mSigninManager;

    private final NativePageNavigationDelegate mPageNavigationDelegate;

    private @Nullable ScrollListener mStreamScrollListener;
    private ContentChangedListener mStreamContentChangedListener;
    private SectionHeader mSectionHeader;
    private MemoryPressureCallback mMemoryPressureCallback;
    private @Nullable SignInPromo mSignInPromo;

    private boolean mFeedEnabled;
    private boolean mHasHeader;
    private boolean mTouchEnabled = true;
    private boolean mStreamContentChanged;
    private boolean mHasHeaderMenu;
    private int mThumbnailWidth;
    private int mThumbnailHeight;
    private int mThumbnailScrollY;

    /** Whether the Feed content is loading. */
    private boolean mIsLoadingFeed;
    /** Cached parameters for recording the histogram of "FeedContentFirstLoadedTime". */
    private boolean mIsInstantStart;
    private long mActivityCreationTimeMs;
    private long mContentFirstAvailableTimeMs;
    // Whether missing a histogram record when onOverviewShownAtLaunch() is called. It is possible
    // that Feed content is still loading at that time and the {@link mContentFirstAvailableTimeMs}
    // hasn't been set yet.
    private boolean mHasPendingUmaRecording;

    /**
     * @param coordinator The {@link FeedSurfaceCoordinator} that interacts with this class.
     * @param snapScrollHelper The {@link SnapScrollHelper} that handles snap scrolling.
     * @param pageNavigationDelegate The {@link NativePageNavigationDelegate} that handles page
     *                               navigation.
     */
    FeedSurfaceMediator(FeedSurfaceCoordinator coordinator,
            @Nullable SnapScrollHelper snapScrollHelper,
            @Nullable NativePageNavigationDelegate pageNavigationDelegate) {
        mCoordinator = coordinator;
        mSnapScrollHelper = snapScrollHelper;
        mSigninManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        mPageNavigationDelegate = pageNavigationDelegate;

        mPrefChangeRegistrar = new PrefChangeRegistrar();
        mHasHeader = mCoordinator.getSectionHeaderView() != null;
        mPrefChangeRegistrar.addObserver(Pref.ENABLE_SNIPPETS, this::updateContent);
        mHasHeaderMenu = FeedFeatures.isReportingUserActions();

        // Check that there is a navigation delegate when using the feed header menu.
        if (mPageNavigationDelegate == null && mHasHeaderMenu) {
            assert false : "Need navigation delegate for header menu";
        }

        initialize();
        // Create the content.
        updateContent();
    }

    /** Clears any dependencies. */
    void destroy() {
        destroyPropertiesForStream();
        mPrefChangeRegistrar.destroy();
        TemplateUrlServiceFactory.get().removeObserver(this);
    }
    @VisibleForTesting
    public void destroyForTesting() {
        destroy();
    }

    private void initialize() {
        if (mSnapScrollHelper == null) return;

        // Listen for layout changes on the NewTabPageView itself to catch changes in scroll
        // position that are due to layout changes after e.g. device rotation. This contrasts with
        // regular scrolling, which is observed through an OnScrollListener.
        mCoordinator.getView().addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    mSnapScrollHelper.handleScroll();
                });
    }

    /** Update the content based on supervised user or enterprise policy. */
    private void updateContent() {
        mFeedEnabled = FeedFeatures.isFeedEnabled();
        if ((mFeedEnabled && mCoordinator.getStream() != null)
                || (!mFeedEnabled && mCoordinator.getScrollViewForPolicy() != null)) {
            return;
        }

        if (mFeedEnabled) {
            mIsLoadingFeed = true;
            mCoordinator.createStream();
            if (mSnapScrollHelper != null) {
                mSnapScrollHelper.setView(mCoordinator.getStream().getView());
            }
            initializePropertiesForStream();
        } else {
            destroyPropertiesForStream();
            mCoordinator.createScrollViewForPolicy();
            if (mSnapScrollHelper != null) {
                mSnapScrollHelper.setView(mCoordinator.getScrollViewForPolicy());
            }
            initializePropertiesForPolicy();
        }
    }

    /**
     * Initialize properties for UI components in the {@link NewTabPage}.
     * TODO(huayinz): Introduce a Model for these properties.
     */
    private void initializePropertiesForStream() {
        Stream stream = mCoordinator.getStream();

        if (mSnapScrollHelper != null) {
            mStreamScrollListener = new ScrollListener() {
                @Override
                public void onScrollStateChanged(int state) {}

                @Override
                public void onScrolled(int dx, int dy) {
                    mSnapScrollHelper.handleScroll();
                }
            };
            stream.addScrollListener(mStreamScrollListener);
        }

        mStreamContentChangedListener = new ContentChangedListener() {
            @Override
            public void onContentChanged() {
                mStreamContentChanged = true;
                if (mSnapScrollHelper != null) mSnapScrollHelper.resetSearchBoxOnScroll(true);

                // Feed v2's background is set to be transparent in {@link
                // FeedSurfaceCoordinator#createStream} to show the Feed placeholder. When first
                // batch of articles are about to show, set recyclerView back to non-transparent.
                // Feed v2 doesn't call onAddFinished(), so we hide placeholder here.
                if (FeedFeatures.isV2Enabled() && mCoordinator.isPlaceholderShown()) {
                    stream.hidePlaceholder();
                }
            }

            @Override
            public void onAddFinished() {
                // Feed v1's background is set to be transparent in {@link
                // FeedSurfaceCoordinator#createStream} to show the Feed placeholder. After first
                // batch of articles finish fade-in animation, set recyclerView back to
                // non-transparent.
                if (!FeedFeatures.isV2Enabled() && mCoordinator.isPlaceholderShown()) {
                    stream.hidePlaceholder();
                }
                if (mContentFirstAvailableTimeMs == 0) {
                    mContentFirstAvailableTimeMs = SystemClock.elapsedRealtime();
                    if (mHasPendingUmaRecording) {
                        maybeRecordContentLoadingTime();
                        mHasPendingUmaRecording = false;
                    }
                }
                mIsLoadingFeed = false;
            }

            @Override
            public void onAddStarting() {
                // Feed v1's sign-in view is set to be invisible in {@link
                // FeedSurfaceCoordinator#getSigninPromoView} if the Feed placeholder is shown. Set
                // sign-in box visible back when Feed articles are about to show.
                if (!FeedFeatures.isV2Enabled() && mCoordinator.isPlaceholderShown()) {
                    mCoordinator.fadeInSigninView();
                }
            }
        };
        stream.addOnContentChangedListener(mStreamContentChangedListener);

        boolean suggestionsVisible = getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE);

        if (mHasHeader) {
            mSectionHeader = new SectionHeader(getSectionHeaderText(suggestionsVisible),
                    suggestionsVisible, this::onSectionHeaderToggled);
            mPrefChangeRegistrar.addObserver(Pref.ARTICLES_LIST_VISIBLE, this::updateSectionHeader);
            TemplateUrlServiceFactory.get().addObserver(this);
            mCoordinator.getSectionHeaderView().setHeader(mSectionHeader);

            if (mHasHeaderMenu) {
                mSectionHeader.setMenuModelList(buildMenuItems());
                mSectionHeader.setListMenuDelegate(this::onItemSelected);
                FeedSurfaceMediator mediator = this;
                HeaderIphScrollListener.Delegate delegate = new HeaderIphScrollListener.Delegate() {
                    @Override
                    public Tracker getFeatureEngagementTracker() {
                        return mCoordinator.getFeatureEngagementTracker();
                    }
                    @Override
                    public Stream getStream() {
                        return mCoordinator.getStream();
                    }
                    @Override
                    public boolean isFeedHeaderPositionInRecyclerViewSuitableForIPH(
                            float headerMaxPosFraction) {
                        return mCoordinator.isFeedHeaderPositionInRecyclerViewSuitableForIPH(
                                headerMaxPosFraction);
                    }
                    @Override
                    public void showMenuIph() {
                        mCoordinator.getSectionHeaderView().showMenuIph(
                                mCoordinator.getUserEducationHelper());
                    }
                    @Override
                    public int getVerticalScrollOffset() {
                        return mediator.getVerticalScrollOffset();
                    }
                    @Override
                    public boolean isFeedExpanded() {
                        return mSectionHeader.isExpanded();
                    }
                    @Override
                    public int getRootViewHeight() {
                        return mCoordinator.getView().getHeight();
                    }
                    @Override
                    public boolean isSignedIn() {
                        return mSigninManager.getIdentityManager().hasPrimaryAccount();
                    }
                };
                mCoordinator.getStream().addScrollListener(new HeaderIphScrollListener(delegate));
                mSigninManager.getIdentityManager().addObserver(this);
            }
        }
        // Show feed if there is no header that would allow user to hide feed.
        // This is currently only relevant for the two panes start surface.
        stream.setStreamContentVisibility(mHasHeader ? mSectionHeader.isExpanded() : true);

        initStreamHeaderViews();

        mMemoryPressureCallback = pressure -> mCoordinator.getStream().trim();
        MemoryPressureListener.addCallback(mMemoryPressureCallback);
    }

    private void initStreamHeaderViews() {
        boolean signInPromoVisible = createSignInPromoIfNeeded();
        View enhancedProtectionPromoView = null;
        if (!signInPromoVisible) {
            enhancedProtectionPromoView = createEnhancedProtectionPromoIfNeeded();
        }
        // We are not going to show two promos at the same time.
        mCoordinator.updateHeaderViews(signInPromoVisible, enhancedProtectionPromoView);
    }

    /**
     * Create and setup the SignInPromo if necessary.
     * @return Whether the SignPromo is visible.
     */
    private boolean createSignInPromoIfNeeded() {
        if (!SignInPromo.shouldCreatePromo()
                || !SigninPromoController.hasNotReachedImpressionLimit(
                        SigninAccessPoint.NTP_CONTENT_SUGGESTIONS)) {
            return false;
        }
        if (mSignInPromo == null) {
            boolean suggestionsVisible = getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE);

            mSignInPromo = new FeedSignInPromo(mSigninManager);
            mSignInPromo.setCanShowPersonalizedSuggestions(suggestionsVisible);
        }
        return mSignInPromo.isVisible();
    }

    private View createEnhancedProtectionPromoIfNeeded() {
        if (mCoordinator.getEnhancedProtectionPromoController() == null) return null;

        View enhancedProtectionPromoView =
                mCoordinator.getEnhancedProtectionPromoController().getPromoView();
        if (enhancedProtectionPromoView != null) {
            mCoordinator.getEnhancedProtectionPromoController()
                    .setEnhancedProtectionPromoStateListener(this);
        }
        return enhancedProtectionPromoView;
    }

    /** Clear any dependencies related to the {@link Stream}. */
    private void destroyPropertiesForStream() {
        Stream stream = mCoordinator.getStream();
        if (stream == null) return;

        if (mStreamScrollListener != null) {
            stream.removeScrollListener(mStreamScrollListener);
            mStreamScrollListener = null;
        }

        stream.removeOnContentChangedListener(mStreamContentChangedListener);
        mStreamContentChangedListener = null;

        MemoryPressureListener.removeCallback(mMemoryPressureCallback);
        mMemoryPressureCallback = null;

        if (mSignInPromo != null) {
            mSignInPromo.destroy();
            mSignInPromo = null;
        }

        mPrefChangeRegistrar.removeObserver(Pref.ARTICLES_LIST_VISIBLE);
        TemplateUrlServiceFactory.get().removeObserver(this);
        mSigninManager.getIdentityManager().removeObserver(this);
    }

    /**
     * Initialize properties for the scroll view shown under supervised user or enterprise policy.
     */
    private void initializePropertiesForPolicy() {
        ScrollView view = mCoordinator.getScrollViewForPolicy();
        if (mSnapScrollHelper != null) {
            view.getViewTreeObserver().addOnScrollChangedListener(mSnapScrollHelper::handleScroll);
        }
    }

    /** Update whether the section header should be expanded and its text contents. */
    private void updateSectionHeader() {
        boolean suggestionsVisible = getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE);
        if (mSectionHeader.isExpanded() != suggestionsVisible) mSectionHeader.toggleHeader();

        mSectionHeader.setHeaderText(getSectionHeaderText(mSectionHeader.isExpanded()));
        if (mHasHeaderMenu) {
            mSectionHeader.setMenuModelList(buildMenuItems());
        }

        if (mSignInPromo != null) {
            mSignInPromo.setCanShowPersonalizedSuggestions(suggestionsVisible);
        }
        if (suggestionsVisible) mCoordinator.getStreamLifecycleManager().activate();
        mStreamContentChanged = true;
        mCoordinator.getSectionHeaderView().updateVisuals();
    }

    /**
     * Callback on section header toggled. This will update the visibility of the Feed and the
     * expand icon on the section header view.
     */
    private void onSectionHeaderToggled() {
        getPrefService().setBoolean(Pref.ARTICLES_LIST_VISIBLE, mSectionHeader.isExpanded());
        mCoordinator.getStream().setStreamContentVisibility(mSectionHeader.isExpanded());
        // TODO(huayinz): Update the section header view through a ModelChangeProcessor.
        mCoordinator.getSectionHeaderView().updateVisuals();
    }

    /** Returns the section header text based on the selected default search engine */
    private String getSectionHeaderText(boolean isExpanded) {
        Resources res = mCoordinator.getSectionHeaderView().getResources();
        final boolean isDefaultSearchEngineGoogle =
                TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle();
        final int sectionHeaderStringId;
        if (mHasHeaderMenu) {
            if (isDefaultSearchEngineGoogle) {
                sectionHeaderStringId =
                        isExpanded ? R.string.ntp_discover_on : R.string.ntp_discover_off;
            } else {
                sectionHeaderStringId = isExpanded ? R.string.ntp_discover_on_branded
                                                   : R.string.ntp_discover_off_branded;
            }
        } else {
            sectionHeaderStringId = isDefaultSearchEngineGoogle
                    ? R.string.ntp_article_suggestions_section_header
                    : R.string.ntp_article_suggestions_section_header_branded;
        }
        return res.getString(sectionHeaderStringId);
    }

    private ModelList buildMenuItems() {
        ModelList itemList = new ModelList();
        int icon_id = 0;
        if (mSigninManager.getIdentityManager().hasPrimaryAccount()) {
            itemList.add(buildMenuListItem(R.string.ntp_manage_my_activity,
                    R.id.ntp_feed_header_menu_item_activity, icon_id));
            itemList.add(buildMenuListItem(R.string.ntp_manage_interests,
                    R.id.ntp_feed_header_menu_item_interest, icon_id));
        }
        itemList.add(buildMenuListItem(
                R.string.learn_more, R.id.ntp_feed_header_menu_item_learn, icon_id));
        if (mSectionHeader.isExpanded()) {
            itemList.add(buildMenuListItem(R.string.ntp_turn_off_feed,
                    R.id.ntp_feed_header_menu_item_toggle_switch, icon_id));
        } else {
            itemList.add(buildMenuListItem(R.string.ntp_turn_on_feed,
                    R.id.ntp_feed_header_menu_item_toggle_switch, icon_id));
        }
        return itemList;
    }

    /**
     * Callback on sign-in promo is dismissed.
     */
    void onSignInPromoDismissed() {
        View view = mCoordinator.getSigninPromoView();
        mSignInPromo.dismiss(removedItemTitle
                -> view.announceForAccessibility(view.getResources().getString(
                        R.string.ntp_accessibility_item_removed, removedItemTitle)));
    }

    /** Whether a new thumbnail should be captured. */
    boolean shouldCaptureThumbnail() {
        return mStreamContentChanged || mCoordinator.getView().getWidth() != mThumbnailWidth
                || mCoordinator.getView().getHeight() != mThumbnailHeight
                || getVerticalScrollOffset() != mThumbnailScrollY;
    }

    /** Reset all the properties for thumbnail capturing after a new thumbnail is captured. */
    void onThumbnailCaptured() {
        mThumbnailWidth = mCoordinator.getView().getWidth();
        mThumbnailHeight = mCoordinator.getView().getHeight();
        mThumbnailScrollY = getVerticalScrollOffset();
        mStreamContentChanged = false;
    }

    /**
     * @return Whether the touch events are enabled on the {@link FeedNewTabPage}.
     * TODO(huayinz): Move this method to a Model once a Model is introduced.
     */
    boolean getTouchEnabled() {
        return mTouchEnabled;
    }

    private PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    // TouchEnabledDelegate interface.

    @Override
    public void setTouchEnabled(boolean enabled) {
        mTouchEnabled = enabled;
    }

    // ScrollDelegate interface.

    @Override
    public boolean isScrollViewInitialized() {
        if (mFeedEnabled) {
            Stream stream = mCoordinator.getStream();
            // During startup the view may not be fully initialized, so we check to see if some
            // basic view properties (height of the RecyclerView) are sane.
            return stream != null && stream.getView().getHeight() > 0;
        } else {
            ScrollView scrollView = mCoordinator.getScrollViewForPolicy();
            return scrollView != null && scrollView.getHeight() > 0;
        }
    }

    @Override
    public int getVerticalScrollOffset() {
        // This method returns a valid vertical scroll offset only when the first header view in the
        // Stream is visible.
        if (!isScrollViewInitialized()) return 0;

        if (mFeedEnabled) {
            int firstChildTop = mCoordinator.getStream().getChildTopAt(0);
            return firstChildTop != Stream.POSITION_NOT_KNOWN ? -firstChildTop : Integer.MIN_VALUE;
        } else {
            return mCoordinator.getScrollViewForPolicy().getScrollY();
        }
    }

    @Override
    public boolean isChildVisibleAtPosition(int position) {
        if (!isScrollViewInitialized()) return false;

        if (mFeedEnabled) {
            return mCoordinator.getStream().isChildAtPositionVisible(position);
        } else {
            ScrollView scrollView = mCoordinator.getScrollViewForPolicy();
            Rect rect = new Rect();
            scrollView.getHitRect(rect);
            return scrollView.getChildAt(position).getLocalVisibleRect(rect);
        }
    }

    @Override
    public void snapScroll() {
        if (mSnapScrollHelper == null) return;
        if (!isScrollViewInitialized()) return;

        int initialScroll = getVerticalScrollOffset();
        int scrollTo = mSnapScrollHelper.calculateSnapPosition(initialScroll);

        // Calculating the snap position should be idempotent.
        assert scrollTo == mSnapScrollHelper.calculateSnapPosition(scrollTo);

        if (mFeedEnabled) {
            mCoordinator.getStream().smoothScrollBy(0, scrollTo - initialScroll);
        } else {
            mCoordinator.getScrollViewForPolicy().smoothScrollBy(0, scrollTo - initialScroll);
        }
    }

    @Override
    public void onTemplateURLServiceChanged() {
        updateSectionHeader();
    }

    @Override
    public void onItemSelected(PropertyModel item) {
        int itemId = item.get(ListMenuItemProperties.MENU_ITEM_ID);
        if (itemId == R.id.ntp_feed_header_menu_item_activity) {
            mPageNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams("https://myactivity.google.com/myactivity?product=50"));
            FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_CLICKED_MY_ACTIVITY);
        } else if (itemId == R.id.ntp_feed_header_menu_item_interest) {
            mPageNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams("https://www.google.com/preferences/interests"));
            FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_CLICKED_MANAGE_INTERESTS);
        } else if (itemId == R.id.ntp_feed_header_menu_item_learn) {
            mPageNavigationDelegate.navigateToHelpPage();
            FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_CLICKED_LEARN_MORE);
        } else if (itemId == R.id.ntp_feed_header_menu_item_toggle_switch) {
            mSectionHeader.toggleHeader();
            FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_TOGGLED_FEED);
            SuggestionsMetrics.recordArticlesListVisible();
        } else {
            assert false : String.format(Locale.ENGLISH,
                                   "Cannot handle action for item in the menu with id %d", itemId);
        }
    }

    @Override
    public void onEnhancedProtectionPromoStateChange() {
        // If the enhanced protection promo has been dismissed, delete it.
        mCoordinator.updateHeaderViews(false, null);
    }

    // IdentityManager.Observer interface.

    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        updateSectionHeader();
    }

    /**
     * The {@link SignInPromo} for the Feed.
     * TODO(huayinz): Update content and visibility through a ModelChangeProcessor.
     */
    private class FeedSignInPromo extends SignInPromo {
        FeedSignInPromo(SigninManager signinManager) {
            super(signinManager);
            maybeUpdateSignInPromo();
        }

        @Override
        protected void setVisibilityInternal(boolean visible) {
            if (isVisible() == visible) return;

            super.setVisibilityInternal(visible);
            mCoordinator.updateHeaderViews(visible, null);
            maybeUpdateSignInPromo();
        }

        @Override
        protected void notifyDataChanged() {
            maybeUpdateSignInPromo();
        }

        /** Update the content displayed in {@link PersonalizedSigninPromoView}. */
        private void maybeUpdateSignInPromo() {
            // Only call #setupPromoViewFromCache() if SignInPromo is visible to avoid potentially
            // blocking the UI thread for several seconds if the accounts cache is not populated
            // yet.
            if (!isVisible()) return;
            if (isUserSignedInButNotSyncing()) {
                SigninPromoUtil.setupSyncPromoViewFromCache(mSigninPromoController,
                        mProfileDataCache, mCoordinator.getSigninPromoView(), null);
            } else {
                SigninPromoUtil.setupSigninPromoViewFromCache(mSigninPromoController,
                        mProfileDataCache, mCoordinator.getSigninPromoView(), null);
            }
        }
    }

    // TODO(huayinz): Return the Model for testing in Coordinator instead once a Model is created.
    @VisibleForTesting
    public SectionHeader getSectionHeaderForTesting() {
        return mSectionHeader;
    }

    @VisibleForTesting
    public SignInPromo getSignInPromoForTesting() {
        return mSignInPromo;
    }

    void onOverviewShownAtLaunch(long activityCreationTimeMs, boolean isInstantStart) {
        assert mActivityCreationTimeMs == 0;
        mActivityCreationTimeMs = activityCreationTimeMs;
        mIsInstantStart = isInstantStart;

        if (!maybeRecordContentLoadingTime() && mIsLoadingFeed) {
            mHasPendingUmaRecording = true;
        }
    }

    private boolean maybeRecordContentLoadingTime() {
        if (mActivityCreationTimeMs == 0 || mContentFirstAvailableTimeMs == 0) return false;

        StartSurfaceConfiguration.recordHistogram(FEED_CONTENT_FIRST_LOADED_TIME_MS_UMA,
                mContentFirstAvailableTimeMs - mActivityCreationTimeMs, mIsInstantStart);
        return true;
    }
}
