// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Handler;
import android.os.SystemClock;
import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.ObserverList;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.feedmanagement.FeedManagementActivity;
import org.chromium.chrome.browser.feed.shared.FeedFeatures;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageLaunchOrigin;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.ntp.ScrollListener;
import org.chromium.chrome.browser.ntp.SnapScrollHelper;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.cards.promo.enhanced_protection.EnhancedProtectionPromoController.EnhancedProtectionPromoStateListener;
import org.chromium.chrome.browser.ntp.snippets.OnSectionHeaderSelectedListener;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderListProperties;
import org.chromium.chrome.browser.ntp.snippets.SectionHeaderProperties;
import org.chromium.chrome.browser.ntp.snippets.SectionType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.ui.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ui.SigninPromoController;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.components.feed.proto.wire.ReliabilityLoggingEnums.DiscoverLaunchResult;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.HashMap;
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
    private static final String TAG = "FeedSurfaceMediator";
    @VisibleForTesting
    public static final String FEED_CONTENT_FIRST_LOADED_TIME_MS_UMA = "FeedContentFirstLoadedTime";
    private static final int INTEREST_FEED_HEADER_POSITION = 0;

    /**
     * Class for storing scroll state of a feed surface.
     */
    public static class ScrollState {
        private static final String TAG = "FeedScrollState";

        private static final String SCROLL_POSITION = "pos";
        private static final String SCROLL_LAST_POSITION = "lpos";
        private static final String SCROLL_OFFSET = "off";
        private static final String TAB_ID = "tabId";

        public int position;
        public int lastPosition;
        public int offset;
        public int tabId;

        /** Turns the fields into json. */
        public String toJson() {
            JSONObject jsonSavedState = new JSONObject();
            try {
                jsonSavedState.put(SCROLL_POSITION, position);
                jsonSavedState.put(SCROLL_LAST_POSITION, lastPosition);
                jsonSavedState.put(SCROLL_OFFSET, offset);
                jsonSavedState.put(TAB_ID, tabId);
                return jsonSavedState.toString();
            } catch (JSONException e) {
                Log.d(TAG, "Unable to write to a JSONObject.");
            }
            return "";
        }

        /** Reads from json to recover a ScrollState object. */
        @Nullable
        static ScrollState fromJson(String json) {
            if (json == null) return null;
            ScrollState result = new ScrollState();
            try {
                JSONObject jsonSavedState = new JSONObject(json);
                result.position = jsonSavedState.getInt(SCROLL_POSITION);
                result.lastPosition = jsonSavedState.getInt(SCROLL_LAST_POSITION);
                result.offset = jsonSavedState.getInt(SCROLL_OFFSET);
                result.tabId = jsonSavedState.getInt(TAB_ID);
            } catch (JSONException e) {
                Log.d(TAG, "Unable to parse a JSONObject from a string.");
                return null;
            }
            return result;
        }
    }

    private class FeedSurfaceHeaderSelectedCallback implements OnSectionHeaderSelectedListener {
        @Override
        public void onSectionHeaderSelected(int index) {
            PropertyListModel<PropertyModel, PropertyKey> headerList =
                    mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY);
            if (index >= headerList.size()) {
                Log.e(TAG, "Attempted to select a Tab with an invalid index");
                return;
            }
            mSectionHeaderModel.set(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY, index);
            Runnable onSelectCallback =
                    headerList.get(index).get(SectionHeaderProperties.ON_SELECT_CALLBACK_KEY);
            if (onSelectCallback != null) {
                onSelectCallback.run();
            }
            bindStream(mTabToStreamMap.get(index));
        }
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
            if (isVisible()) {
                mSigninPromoController.setUpSyncPromoView(mProfileDataCache,
                        mCoordinator.getSigninPromoView().findViewById(
                                R.id.signin_promo_view_container),
                        null);
            }
        }
    }

    @VisibleForTesting
    static void setPrefForTest(PrefChangeRegistrar prefChangeRegistrar, PrefService prefService) {
        sTestPrefChangeRegistar = prefChangeRegistrar;
        sPrefServiceForTest = prefService;
    }

    private static PrefChangeRegistrar sTestPrefChangeRegistar;
    private static PrefService sPrefServiceForTest;

    private final FeedSurfaceCoordinator mCoordinator;
    private final Context mContext;
    private final @Nullable SnapScrollHelper mSnapScrollHelper;
    private final PrefChangeRegistrar mPrefChangeRegistrar;
    private final SigninManager mSigninManager;
    private final PropertyModel mSectionHeaderModel;

    private final NativePageNavigationDelegate mPageNavigationDelegate;

    private @Nullable RecyclerView.OnScrollListener mStreamScrollListener;
    private final ObserverList<ScrollListener> mScrollListeners =
            new ObserverList<ScrollListener>();
    private ContentChangedListener mStreamContentChangedListener;
    private MemoryPressureCallback mMemoryPressureCallback;
    private @Nullable SignInPromo mSignInPromo;
    private RecyclerViewAnimationFinishDetector mRecyclerViewAnimationFinishDetector =
            new RecyclerViewAnimationFinishDetector();

    private boolean mFeedEnabled;
    private boolean mHasHeader;
    private boolean mTouchEnabled = true;
    private boolean mStreamContentChanged;
    private int mThumbnailWidth;
    private int mThumbnailHeight;
    private int mThumbnailScrollY;
    private int mRestoreTabId;

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
    private ScrollState mRestoreScrollState;

    private final HashMap<Integer, Stream> mTabToStreamMap = new HashMap<>();
    private Stream mCurrentStream;

    /**
     * @param coordinator The {@link FeedSurfaceCoordinator} that interacts with this class.
     * @param context The current context.
     * @param snapScrollHelper The {@link SnapScrollHelper} that handles snap scrolling.
     * @param pageNavigationDelegate The {@link NativePageNavigationDelegate} that handles page
     *         navigation.
     * @param headerModel The {@link PropertyModel} that contains this mediator should work with.
     * @param openingTabId The {@link FeedSurfaceCoordinator.StreamTabId} the feed should open to.
     */
    FeedSurfaceMediator(FeedSurfaceCoordinator coordinator, Context context,
            @Nullable SnapScrollHelper snapScrollHelper,
            @Nullable NativePageNavigationDelegate pageNavigationDelegate,
            PropertyModel headerModel, @FeedSurfaceCoordinator.StreamTabId int openingTabId) {
        mCoordinator = coordinator;
        mContext = context;
        mSnapScrollHelper = snapScrollHelper;
        mSigninManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        mPageNavigationDelegate = pageNavigationDelegate;
        mRestoreTabId = openingTabId;

        if (sTestPrefChangeRegistar != null) {
            mPrefChangeRegistrar = sTestPrefChangeRegistar;
        } else {
            mPrefChangeRegistrar = new PrefChangeRegistrar();
        }
        mHasHeader = headerModel != null;
        mPrefChangeRegistrar.addObserver(Pref.ENABLE_SNIPPETS, this::updateContent);

        // Check that there is a navigation delegate when using the feed header menu.
        if (mPageNavigationDelegate == null) {
            assert false : "Need navigation delegate for header menu";
        }

        mSectionHeaderModel = headerModel;

        // This works around the bug that the out-of-screen toolbar is not brought back together
        // with the new tab page view when it slides down. This is because the RecyclerView
        // animation may not finish when content changed event is triggered and thus the new tab
        // page layout view may still be partially off screen.
        mStreamContentChangedListener = contents
                -> mRecyclerViewAnimationFinishDetector.runWhenAnimationComplete(
                        this::onContentsChanged);

        initialize();
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
    void updateContent() {
        mFeedEnabled = FeedFeatures.isFeedEnabled();
        if ((mFeedEnabled && !mTabToStreamMap.isEmpty())
                || (!mFeedEnabled && mCoordinator.getScrollViewForPolicy() != null)) {
            return;
        }

        if (mFeedEnabled) {
            mIsLoadingFeed = true;
            mCoordinator.createStream();
            if (mSnapScrollHelper != null) {
                mSnapScrollHelper.setView(mCoordinator.getRecyclerView());
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

    /** Gets the current state, for restoring later. */
    String getSavedInstanceString() {
        ScrollState state = new ScrollState();
        int tabId = mSectionHeaderModel.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY);
        state.tabId = tabId;
        LinearLayoutManager layoutManager =
                (LinearLayoutManager) mCoordinator.getRecyclerView().getLayoutManager();
        if (layoutManager != null) {
            state.position = layoutManager.findFirstVisibleItemPosition();
            state.lastPosition = layoutManager.findLastVisibleItemPosition();
            if (state.position != RecyclerView.NO_POSITION) {
                View firstVisibleView = layoutManager.findViewByPosition(state.position);
                if (firstVisibleView != null) {
                    state.offset = firstVisibleView.getTop();
                }
            }
        }
        return state.toJson();
    }

    /** Restores a previously saved state. */
    void restoreSavedInstanceState(String json) {
        ScrollState state = ScrollState.fromJson(json);
        if (state == null) return;
        mRestoreTabId = state.tabId;
        if (mSectionHeaderModel != null) {
            mSectionHeaderModel.set(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY, state.tabId);
        }
        if (mCurrentStream == null) {
            mRestoreScrollState = state;
        } else {
            mCurrentStream.restoreSavedInstanceState(state);
        }
    }

    /**
     * Sets the current tab to {@code tabId}.
     *
     * <p>Called when the the mediator is already initialized in Start Surface, but the feed is
     * being shown again with a different {@link NewTabPageLaunchOrigin}.
     */
    void setTabId(@FeedSurfaceCoordinator.StreamTabId int tabId) {
        if (mTabToStreamMap.size() <= tabId) tabId = 0;
        mSectionHeaderModel.set(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY, tabId);
    }

    /**
     * Initialize properties for UI components in the {@link NewTabPage}.
     * TODO(huayinz): Introduce a Model for these properties.
     */
    private void initializePropertiesForStream() {
        if (mHasHeader) {
            mSectionHeaderModel.set(SectionHeaderListProperties.ON_TAB_SELECTED_CALLBACK_KEY,
                    new FeedSurfaceHeaderSelectedCallback());

            mPrefChangeRegistrar.addObserver(Pref.ARTICLES_LIST_VISIBLE, this::updateSectionHeader);
            TemplateUrlServiceFactory.get().addObserver(this);

            boolean suggestionsVisible = getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE);
            mSectionHeaderModel.set(
                    SectionHeaderListProperties.IS_SECTION_ENABLED_KEY, suggestionsVisible);
            // Build menu after section enabled key is set.
            mFeedMenuModel = buildMenuItems();

            addHeaderAndStream(
                    getInterestFeedHeaderText(suggestionsVisible), null, mCoordinator.getStream());

            mCoordinator.initializeIph();
            mSigninManager.getIdentityManager().addObserver(this);

            mSectionHeaderModel.set(
                    SectionHeaderListProperties.MENU_MODEL_LIST_KEY, mFeedMenuModel);
            mSectionHeaderModel.set(
                    SectionHeaderListProperties.MENU_DELEGATE_KEY, this::onItemSelected);

            setUpWebFeedTab();

            // Set the current tab index to what restoreSavedInstanceState had.
            if (mTabToStreamMap.size() <= mRestoreTabId) mRestoreTabId = 0;
            mSectionHeaderModel.set(
                    SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY, mRestoreTabId);
        } else {
            // Show feed if there is no header that would allow user to hide feed.
            // This is currently only relevant for the two panes start surface.
            // TODO(chili): verify start surface wants to show feed even if user previously turned
            // off feed (?).
            mSectionHeaderModel.set(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY, true);
            // Add to tab map.
            mTabToStreamMap.put(0, mCoordinator.getStream());
            mSectionHeaderModel.set(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY, 0);
        }

        if (mCoordinator.isActive()
                && mSectionHeaderModel.get(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY)) {
            bindStream(mTabToStreamMap.get(
                    mSectionHeaderModel.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY)));
        } else {
            unbindStream();
        }

        mStreamScrollListener = new RecyclerView.OnScrollListener() {
            @Override
            public void onScrollStateChanged(@NonNull RecyclerView recyclerView, int newState) {
                for (ScrollListener listener : mScrollListeners) {
                    listener.onScrollStateChanged(newState);
                }
            }

            @Override
            public void onScrolled(RecyclerView v, int dx, int dy) {
                if (mSnapScrollHelper != null) {
                    mSnapScrollHelper.handleScroll();
                }
                for (ScrollListener listener : mScrollListeners) {
                    listener.onScrolled(dx, dy);
                }
            }
        };
        mCoordinator.getRecyclerView().addOnScrollListener(mStreamScrollListener);

        initStreamHeaderViews();

        mMemoryPressureCallback =
                pressure -> mCoordinator.getRecyclerView().getRecycledViewPool().clear();
        MemoryPressureListener.addCallback(mMemoryPressureCallback);
    }

    void addScrollListener(ScrollListener listener) {
        mScrollListeners.addObserver(listener);
    }

    void removeScrollListener(ScrollListener listener) {
        mScrollListeners.removeObserver(listener);
    }

    private void addHeaderAndStream(
            String headerText, @Nullable String accessibilityTextUnreadContent, Stream stream) {
        PropertyModel headerModel = SectionHeaderProperties.createSectionHeader(headerText);
        mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY).add(headerModel);
        int tabId =
                mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY).size() - 1;

        // Update UNREAD_CONTENT_KEY and HEADER_ACCESSIBILITY_TEXT_KEY now, and any time
        // hasUnreadContent() changes.
        Callback<Boolean> callback = hasUnreadContent -> {
            headerModel.set(SectionHeaderProperties.UNREAD_CONTENT_KEY, hasUnreadContent);
        };
        callback.onResult(stream.hasUnreadContent().addObserver(callback));

        mTabToStreamMap.put(tabId, stream);
    }

    private int getTabIdForSection(@SectionType int sectionType) {
        for (int tabId : mTabToStreamMap.keySet()) {
            if (mTabToStreamMap.get(tabId).getSectionType() == sectionType) {
                return tabId;
            }
        }
        return -1;
    }

    /** Adds or removes the WebFeed tab, depending on whether it should be shown. */
    private void setUpWebFeedTab() {
        // Skip if the for-you tab hasn't been added yet.
        if (getTabIdForSection(SectionType.FOR_YOU_FEED) == -1) {
            return;
        }
        int tabId = getTabIdForSection(SectionType.WEB_FEED);
        boolean hasWebFeedTab = tabId != -1;
        boolean shouldHaveWebFeedTab = mHasHeader && FeedFeatures.isWebFeedUIEnabled();
        if (hasWebFeedTab == shouldHaveWebFeedTab) return;
        if (shouldHaveWebFeedTab) {
            addHeaderAndStream(mContext.getResources().getString(R.string.ntp_following),
                    mContext.getResources().getString(
                            R.string.accessibility_ntp_following_unread_content),

                    mCoordinator.createFeedStream(/* isInterestFeed = */ false));
        }
    }

    /**
     * Binds a stream to the {@link NtpListContentManager}. Unbinds currently active stream if
     * different from new stream. Once bound, the stream can add/remove contents.
     */
    @VisibleForTesting
    void bindStream(Stream stream) {
        if (mCurrentStream == stream) return;
        if (mCurrentStream != null) {
            unbindStream();
        }
        mCurrentStream = stream;
        mCurrentStream.addOnContentChangedListener(mStreamContentChangedListener);

        if (FeedFeatures.isAutoScrollToTopEnabled() && mRestoreScrollState == null) {
            mRestoreScrollState = getScrollStateForAutoScrollToTop();
        }

        mCurrentStream.bind(mCoordinator.getRecyclerView(), mCoordinator.getContentManager(),
                mRestoreScrollState, mCoordinator.getSurfaceScope(),
                mCoordinator.getHybridListRenderer(), mCoordinator.getLaunchReliabilityLogger());
        mRestoreScrollState = null;
        mCoordinator.getHybridListRenderer().onSurfaceOpened();
    }

    void onContentsChanged() {
        if (mSnapScrollHelper != null) mSnapScrollHelper.resetSearchBoxOnScroll(true);

        if (mContentFirstAvailableTimeMs == 0) {
            mContentFirstAvailableTimeMs = SystemClock.elapsedRealtime();
            if (mHasPendingUmaRecording) {
                maybeRecordContentLoadingTime();
                mHasPendingUmaRecording = false;
            }
        }
        mIsLoadingFeed = false;
        mStreamContentChanged = true;
    }

    private void unbindStream() {
        if (mCurrentStream == null) return;
        mCoordinator.getHybridListRenderer().onSurfaceClosed();
        mCurrentStream.unbind();
        mCurrentStream.removeOnContentChangedListener(mStreamContentChangedListener);
        mCurrentStream = null;

        FeedLaunchReliabilityLogger launchLogger = mCoordinator.getLaunchReliabilityLogger();
        if (launchLogger.isLaunchInProgress()) {
            // This is the catch-all feed launch end event to ensure a complete flow is logged
            // even if we don't know a more specific reason for the stream unbinding.
            launchLogger.logLaunchFinished(
                    System.nanoTime(), DiscoverLaunchResult.FRAGMENT_STOPPED.getNumber());
        }
    }

    void onSurfaceOpened() {
        rebindStream();
    }

    void onSurfaceClosed() {
        unbindStream();
    }

    /** @return The stream that represents the 1st tab. */
    Stream getFirstStream() {
        if (mTabToStreamMap.isEmpty()) return null;
        return mTabToStreamMap.get(0);
    }

    @VisibleForTesting
    Stream getCurrentStreamForTesting() {
        return mCurrentStream;
    }

    private void rebindStream() {
        // If a stream is already bound, then do nothing.
        if (mCurrentStream != null) return;
        // If feed shouldn't be shown, do nothing.
        if (!mSectionHeaderModel.get(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY)) return;
        // Find the stream that should be bound and bind it. If no stream matches, then we haven't
        // fully set up yet. This will be taken care of by setup.
        Stream stream = mTabToStreamMap.get(
                mSectionHeaderModel.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
        if (stream != null) {
            bindStream(stream);
        }
    }

    /**
     * Notifies a bound stream of new header count number.
     * @param newHeaderCount Number of headers in the {@link RecyclerView}.
     */
    void notifyHeadersChanged(int newHeaderCount) {
        if (mCurrentStream != null) {
            mCurrentStream.notifyNewHeaderCount(newHeaderCount);
        }
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
     * @return Whether the SignPromo should be visible.
     */
    private boolean createSignInPromoIfNeeded() {
        if (!SignInPromo.shouldCreatePromo()
                || !SigninPromoController.hasNotReachedImpressionLimit(
                        SigninAccessPoint.NTP_CONTENT_SUGGESTIONS)
                || SigninPromoController.shouldHideSyncPromoForNTP(
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
        if (mTabToStreamMap.isEmpty()) return;

        if (mStreamScrollListener != null) {
            mCoordinator.getRecyclerView().removeOnScrollListener(mStreamScrollListener);
            mStreamScrollListener = null;
        }

        MemoryPressureListener.removeCallback(mMemoryPressureCallback);
        mMemoryPressureCallback = null;

        if (mSignInPromo != null) {
            mSignInPromo.destroy();
            mSignInPromo = null;
        }

        unbindStream();
        for (Stream s : mTabToStreamMap.values()) {
            s.removeOnContentChangedListener(mStreamContentChangedListener);
            s.destroy();
        }
        mTabToStreamMap.clear();
        mStreamContentChangedListener = null;

        mPrefChangeRegistrar.removeObserver(Pref.ARTICLES_LIST_VISIBLE);
        TemplateUrlServiceFactory.get().removeObserver(this);
        mSigninManager.getIdentityManager().removeObserver(this);

        PropertyListModel<PropertyModel, PropertyKey> headerList =
                mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY);
        if (headerList.size() > 0) {
            headerList.removeRange(0, headerList.size());
        }

        if (mCoordinator.getSurfaceScope() != null) {
            mCoordinator.getSurfaceScope().getFeedLaunchReliabilityLogger().cancelPendingEvents();
        }
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
    void updateSectionHeader() {
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
        if (suggestionsVisible) mCoordinator.getSurfaceLifecycleManager().show();
        mStreamContentChanged = true;

        // If feed turned on, we bind the last stream that was visible. Else unbind it.
        if (suggestionsVisible) {
            rebindStream();
        } else {
            unbindStream();
        }
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
        // TODO(chili): This toggledArticlesListVisible should probably be in a different JNI class
        //  rather than a stream.
        if (mCurrentStream != null) {
            mCurrentStream.toggledArticlesListVisible(isExpanded);
        } else {
            mCoordinator.getStream().toggledArticlesListVisible(isExpanded);
        }
    }

    /** Returns the interest feed header text based on the selected default search engine */
    private String getInterestFeedHeaderText(boolean isExpanded) {
        Resources res = mContext.getResources();
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
        if (FeedServiceBridge.isAutoplayEnabled()) {
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
        if (sPrefServiceForTest != null) return sPrefServiceForTest;
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
            return mCoordinator.getRecyclerView().getHeight() > 0;
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
            if (!isChildVisibleAtPosition(0)) {
                return Integer.MIN_VALUE;
            }

            LinearLayoutManager layoutManager =
                    (LinearLayoutManager) mCoordinator.getRecyclerView().getLayoutManager();
            if (layoutManager == null) {
                return Integer.MIN_VALUE;
            }

            View view = layoutManager.findViewByPosition(0);
            if (view == null) {
                return Integer.MIN_VALUE;
            }

            return -view.getTop();
        } else {
            return mCoordinator.getScrollViewForPolicy().getScrollY();
        }
    }

    @Override
    public boolean isChildVisibleAtPosition(int position) {
        if (!isScrollViewInitialized()) return false;

        if (mFeedEnabled) {
            LinearLayoutManager layoutManager =
                    (LinearLayoutManager) mCoordinator.getRecyclerView().getLayoutManager();
            if (layoutManager == null) {
                return false;
            }

            int firstItemPosition = layoutManager.findFirstVisibleItemPosition();
            int lastItemPosition = layoutManager.findLastVisibleItemPosition();
            if (firstItemPosition == RecyclerView.NO_POSITION
                    || lastItemPosition == RecyclerView.NO_POSITION) {
                return false;
            }

            return firstItemPosition <= position && position <= lastItemPosition;
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
            mCoordinator.getRecyclerView().smoothScrollBy(0, scrollTo - initialScroll);
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

    private ScrollState getScrollStateForAutoScrollToTop() {
        ScrollState state = new ScrollState();
        state.position = 1;
        state.lastPosition = 5;
        return state;
    }

    // Detects animation finishes in RecyclerView.
    // https://stackoverflow.com/questions/33710605/detect-animation-finish-in-androids-recyclerview
    private class RecyclerViewAnimationFinishDetector
            implements RecyclerView.ItemAnimator.ItemAnimatorFinishedListener {
        private Runnable mFinishedCallback;

        /**
         * Asynchronously waits for the animation to finish. If there's already a callback waiting,
         * this replaces the existing callback.
         *
         * @param finishedCallback Callback to invoke when the animation finishes.
         */
        public void runWhenAnimationComplete(Runnable finishedCallback) {
            if (mCoordinator.getRecyclerView() == null) {
                return;
            }
            mFinishedCallback = finishedCallback;

            // The RecyclerView has not started animating yet, so post a message to the
            // message queue that will be run after the RecyclerView has started animating.
            new Handler().post(() -> { checkFinish(); });
        }

        private void checkFinish() {
            RecyclerView recyclerView = mCoordinator.getRecyclerView();

            if (recyclerView != null && recyclerView.isAnimating()) {
                // The RecyclerView is still animating, try again when the animation has finished.
                recyclerView.getItemAnimator().isRunning(this);
                return;
            }

            // The RecyclerView has animated all it's views.
            onFinished();
        }

        private void onFinished() {
            if (mFinishedCallback != null) {
                mFinishedCallback.run();
                mFinishedCallback = null;
            }
        }

        @Override
        public void onAnimationsFinished() {
            // There might still be more items that will be animated after this one.
            new Handler().post(() -> { checkFinish(); });
        }
    }
}
