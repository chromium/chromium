// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.content.Context;
import android.content.Intent;
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
import org.chromium.chrome.browser.app.feedmanagement.FeedManagementActivity;
import org.chromium.chrome.browser.feed.shared.FeedFeatures;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.ScrollListener;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.cards.promo.enhanced_protection.EnhancedProtectionPromoController.EnhancedProtectionPromoStateListener;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderListProperties;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderProperties;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.ui.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ui.SigninPromoController;
import org.chromium.chrome.browser.signin.ui.SigninPromoUtil;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.MVCListAdapter;
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
    private static final int INTEREST_FEED_HEADER_POSITION = 0;

    private final FeedSurfaceCoordinator mCoordinator;
    private final Context mContext;
    private final @Nullable SnapScrollHelper mSnapScrollHelper;
    private final PrefChangeRegistrar mPrefChangeRegistrar;
    private final SigninManager mSigninManager;
    private final PropertyModel mSectionHeaderModel;

    private final NativePageNavigationDelegate mPageNavigationDelegate;

    private @Nullable ScrollListener mStreamScrollListener;
    private ContentChangedListener mStreamContentChangedListener;
    private MemoryPressureCallback mMemoryPressureCallback;
    private @Nullable SignInPromo mSignInPromo;

    private boolean mFeedEnabled;
    private boolean mHasHeader;
    private boolean mTouchEnabled = true;
    private boolean mStreamContentChanged;
    private int mThumbnailWidth;
    private int mThumbnailHeight;
    private int mThumbnailScrollY;

    /** The model representing feed-related cog menu items. */
    private ModelList mFeedMenuModel;

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
    private int mToggleswitchMenuIndex;

    /**
     * @param coordinator The {@link FeedSurfaceCoordinator} that interacts with this class.
     * @param context The current context.
     * @param snapScrollHelper The {@link SnapScrollHelper} that handles snap scrolling.
     * @param pageNavigationDelegate The {@link NativePageNavigationDelegate} that handles page
     *         navigation.
     * @param propertyModel The {@link PropertyModel} that contains this mediator should work with.
     */
    FeedSurfaceMediator(FeedSurfaceCoordinator coordinator, Context context,
            @Nullable SnapScrollHelper snapScrollHelper,
            @Nullable NativePageNavigationDelegate pageNavigationDelegate,
            PropertyModel propertyModel) {
        mCoordinator = coordinator;
        mContext = context;
        mSnapScrollHelper = snapScrollHelper;
        mSigninManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        mPageNavigationDelegate = pageNavigationDelegate;

        mPrefChangeRegistrar = new PrefChangeRegistrar();
        mHasHeader = mCoordinator.getSectionHeaderView() != null;
        mPrefChangeRegistrar.addObserver(Pref.ENABLE_SNIPPETS, this::updateContent);

        // Check that there is a navigation delegate when using the feed header menu.
        if (mPageNavigationDelegate == null) {
            assert false : "Need navigation delegate for header menu";
        }

        mSectionHeaderModel = propertyModel;

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
        if (mSectionHeaderModel != null) {
            mSectionHeaderModel.set(
                    SectionHeaderListProperties.ON_TAB_SELECTED_CALLBACK_KEY, this::onTabSelected);
        }
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

        if (mSnapScrollHelper != null && stream != null) {
            mStreamScrollListener = new ScrollListener() {
                @Override
                public void onScrollStateChanged(int state) {}

                @Override
                public void onScrolled(int dx, int dy) {
                    mSnapScrollHelper.handleScroll();
                }

                @Override
                public void onHeaderOffsetChanged(int verticalOffset) {}
            };
            stream.addScrollListener(mStreamScrollListener);
        }

        mStreamContentChangedListener = new ContentChangedListener() {
            @Override
            public void onContentChanged() {
                mStreamContentChanged = true;
                if (mSnapScrollHelper != null) mSnapScrollHelper.resetSearchBoxOnScroll(true);

                if (mContentFirstAvailableTimeMs == 0) {
                    mContentFirstAvailableTimeMs = SystemClock.elapsedRealtime();
                    if (mHasPendingUmaRecording) {
                        maybeRecordContentLoadingTime();
                        mHasPendingUmaRecording = false;
                    }
                }
                mIsLoadingFeed = false;

                // Feed's background is set to be transparent in {@link
                // FeedSurfaceCoordinator#createStream} to show the Feed placeholder. When first
                // batch of articles are about to show, set recyclerView back to non-transparent.
                if (mCoordinator.isPlaceholderShown()) {
                    stream.hidePlaceholder();
                }
            }
        };
        stream.addOnContentChangedListener(mStreamContentChangedListener);

        if (mHasHeader) {
            mPrefChangeRegistrar.addObserver(Pref.ARTICLES_LIST_VISIBLE, this::updateSectionHeader);
            TemplateUrlServiceFactory.get().addObserver(this);

            boolean suggestionsVisible = getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE);
            mSectionHeaderModel.set(
                    SectionHeaderListProperties.IS_SECTION_ENABLED_KEY, suggestionsVisible);
            // Build menu after section enabled key is set.
            mFeedMenuModel = buildMenuItems();

            PropertyModel interestFeedHeader = SectionHeaderProperties.createSectionHeader(
                    getInterestFeedHeaderText(suggestionsVisible));
            mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                    .add(interestFeedHeader);

            mCoordinator.initializeIph();
            mSigninManager.getIdentityManager().addObserver(this);

            mSectionHeaderModel.set(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY, 0);
            mSectionHeaderModel.set(
                    SectionHeaderListProperties.MENU_MODEL_LIST_KEY, mFeedMenuModel);
            mSectionHeaderModel.set(
                    SectionHeaderListProperties.MENU_DELEGATE_KEY, this::onItemSelected);

            if (FeedFeatures.isWebFeedUIEnabled()) {
                PropertyModel webFeedHeader = SectionHeaderProperties.createSectionHeader(
                        mCoordinator.getSectionHeaderView().getResources().getString(
                                R.string.ntp_following));
                mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                        .add(webFeedHeader);
            }
        }
        // Show feed if there is no header that would allow user to hide feed.
        // This is currently only relevant for the two panes start surface.
        stream.setStreamContentVisibility(mHasHeader
                        ? mSectionHeaderModel.get(
                                SectionHeaderListProperties.IS_SECTION_ENABLED_KEY)
                        : true);

        initStreamHeaderViews();

        mMemoryPressureCallback = pressure -> stream.trim();
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

    /**
     * Update whether the section header should be expanded.
     *
     * Called when a settings change or update to this/another NTP caused the feed to show/hide.
     */
    private void updateSectionHeader() {
        boolean suggestionsVisible = getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE);
        mSectionHeaderModel.set(
                SectionHeaderListProperties.IS_SECTION_ENABLED_KEY, suggestionsVisible);

        if (!FeedFeatures.isWebFeedUIEnabled()) {
            mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                    .get(INTEREST_FEED_HEADER_POSITION)
                    .set(SectionHeaderProperties.HEADER_TEXT_KEY,
                            getInterestFeedHeaderText(suggestionsVisible));
        }

        // Update toggleswitch item, which is last item in list.
        mFeedMenuModel.update(mToggleswitchMenuIndex, getMenuToggleSwitch(suggestionsVisible, 0));

        if (mSignInPromo != null) {
            mSignInPromo.setCanShowPersonalizedSuggestions(suggestionsVisible);
        }
        if (suggestionsVisible) mCoordinator.getStreamLifecycleManager().activate();
        mStreamContentChanged = true;

        // Update Feed stream visibility.
        mCoordinator.getStream().setStreamContentVisibility(suggestionsVisible);
    }

    /**
     * Callback on section header toggled. This will update the visibility of the Feed and the
     * expand icon on the section header view.
     */
    private void onSectionHeaderToggled() {
        boolean isExpanded =
                !mSectionHeaderModel.get(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY);

        // Record in prefs and UMA.
        // Model and stream visibility set in {@link #updateSectionHeader}
        // which is called by the prefService observer.
        getPrefService().setBoolean(Pref.ARTICLES_LIST_VISIBLE, isExpanded);
        FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_TOGGLED_FEED);
        SuggestionsMetrics.recordArticlesListVisible();
        mCoordinator.getStream().toggledArticlesListVisible(isExpanded);
    }

    /** Returns the interest feed header text based on the selected default search engine */
    private String getInterestFeedHeaderText(boolean isExpanded) {
        Resources res = mCoordinator.getSectionHeaderView().getResources();
        final boolean isDefaultSearchEngineGoogle =
                TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle();
        final int sectionHeaderStringId;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_FEED)) {
            sectionHeaderStringId = R.string.ntp_for_you;
        } else if (isDefaultSearchEngineGoogle) {
            sectionHeaderStringId =
                    isExpanded ? R.string.ntp_discover_on : R.string.ntp_discover_off;
        } else {
            sectionHeaderStringId = isExpanded ? R.string.ntp_discover_on_branded
                                               : R.string.ntp_discover_off_branded;
        }

        return res.getString(sectionHeaderStringId);
    }

    private ModelList buildMenuItems() {
        ModelList itemList = new ModelList();
        int iconId = 0;
        if (mSigninManager.getIdentityManager().hasPrimaryAccount()) {
            if (FeedFeatures.isWebFeedUIEnabled()) {
                itemList.add(buildMenuListItem(
                        R.string.ntp_manage_feed, R.id.ntp_feed_header_menu_item_manage, iconId));
            } else {
                itemList.add(buildMenuListItem(R.string.ntp_manage_my_activity,
                        R.id.ntp_feed_header_menu_item_activity, iconId));
                itemList.add(buildMenuListItem(R.string.ntp_manage_interests,
                        R.id.ntp_feed_header_menu_item_interest, iconId));
            }
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_V2_HEARTS)) {
                itemList.add(buildMenuListItem(R.string.ntp_manage_reactions,
                        R.id.ntp_feed_header_menu_item_reactions, iconId));
            }
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_V2_AUTOPLAY)) {
            itemList.add(buildMenuListItem(
                    R.string.ntp_manage_autoplay, R.id.ntp_feed_header_menu_item_autoplay, iconId));
        }
        itemList.add(buildMenuListItem(
                R.string.learn_more, R.id.ntp_feed_header_menu_item_learn, iconId));
        itemList.add(getMenuToggleSwitch(
                mSectionHeaderModel.get(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY),
                iconId));
        mToggleswitchMenuIndex = itemList.size() - 1;
        return itemList;
    }

    /**
     * Returns the menu list item that represents turning the feed on/off.
     *
     * @param isEnabled Whether the feed section is currently enabled.
     * @param iconId IconId for the list item if any.
     */
    private MVCListAdapter.ListItem getMenuToggleSwitch(boolean isEnabled, int iconId) {
        if (isEnabled) {
            return buildMenuListItem(R.string.ntp_turn_off_feed,
                    R.id.ntp_feed_header_menu_item_toggle_switch, iconId);
        }
        return buildMenuListItem(
                R.string.ntp_turn_on_feed, R.id.ntp_feed_header_menu_item_toggle_switch, iconId);
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
        Stream stream = mCoordinator.getStream();
        if (itemId == R.id.ntp_feed_header_menu_item_manage) {
            Intent intent = new Intent(mContext, FeedManagementActivity.class);
            mContext.startActivity(intent);
        } else if (itemId == R.id.ntp_feed_header_menu_item_activity) {
            mPageNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams("https://myactivity.google.com/myactivity?product=50"));
            if (stream != null) {
                stream.recordActionManageActivity();
            }
            FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_CLICKED_MY_ACTIVITY);
        } else if (itemId == R.id.ntp_feed_header_menu_item_interest) {
            mPageNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams("https://www.google.com/preferences/interests"));
            if (stream != null) {
                stream.recordActionManageInterests();
            }
            FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_CLICKED_MANAGE_INTERESTS);
        } else if (itemId == R.id.ntp_feed_header_menu_item_reactions) {
            mPageNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams("https://www.google.com/search/contributions/reactions"));
            if (stream != null) {
                stream.recordActionManageReactions();
            }
            FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_CLICKED_MANAGE_INTERESTS);
        } else if (itemId == R.id.ntp_feed_header_menu_item_autoplay) {
            mCoordinator.launchAutoplaySettings();
            FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_CLICKED_MANAGE_AUTOPLAY);
        } else if (itemId == R.id.ntp_feed_header_menu_item_learn) {
            mPageNavigationDelegate.navigateToHelpPage();
            if (stream != null) {
                stream.recordActionLearnMore();
            }
            FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_CLICKED_LEARN_MORE);
        } else if (itemId == R.id.ntp_feed_header_menu_item_toggle_switch) {
            onSectionHeaderToggled();
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

    private void onTabSelected(int index) {
        mSectionHeaderModel.set(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY, index);
        Runnable onSelectCallback =
                mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                        .get(index)
                        .get(SectionHeaderProperties.ON_SELECT_CALLBACK_KEY);
        if (onSelectCallback != null) {
            onSelectCallback.run();
        }
        // TODO(chili): Register observers for new datastream; de-register observer for old
        // datastream.
    }
}
