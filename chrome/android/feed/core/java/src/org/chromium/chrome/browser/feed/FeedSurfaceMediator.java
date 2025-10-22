// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Handler;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.ObserverList;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.feed.feedmanagement.FeedManagementActivity;
import org.chromium.chrome.browser.feed.FeedSurfaceProvider.RestoringState;
import org.chromium.chrome.browser.feed.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator;
import org.chromium.chrome.browser.feed.sections.OnSectionHeaderSelectedListener;
import org.chromium.chrome.browser.feed.sections.SectionHeaderListProperties;
import org.chromium.chrome.browser.feed.sections.SectionHeaderProperties;
import org.chromium.chrome.browser.feed.sections.ViewVisibility;
import org.chromium.chrome.browser.feed.sort_ui.FeedOptionsCoordinator;
import org.chromium.chrome.browser.feed.sort_ui.FeedOptionsCoordinator.OptionChangedListener;
import org.chromium.chrome.browser.feed.v2.ContentOrder;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gesturenav.GestureNavigationUtils;
import org.chromium.chrome.browser.new_tab_url.DseNewTabUrlManager;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.SyncPromoController;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.signin_promo.NtpSigninPromoDelegate;
import org.chromium.chrome.browser.ui.signin.signin_promo.SigninPromoCoordinator;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;
import org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger.ClosedReason;
import org.chromium.chrome.browser.xsurface.feed.StreamType;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Objects;

/**
 * A mediator for the {@link FeedSurfaceCoordinator} responsible for interacting with the native
 * library and handling business logic.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
@NullMarked
public class FeedSurfaceMediator
        implements FeedSurfaceScrollDelegate,
                TouchEnabledDelegate,
                TemplateUrlServiceObserver,
                ListMenu.Delegate,
                IdentityManager.Observer,
                OptionChangedListener {

    // Position of the in-feed header for the for-you feed.
    private static final int PRIMARY_FEED_HEADER_POSITION = 0;

    private class FeedSurfaceHeaderSelectedCallback implements OnSectionHeaderSelectedListener {
        @Override
        public void onSectionHeaderSelected(int index) {
            switchToStream(index);
        }

        @Override
        public void onSectionHeaderUnselected(int index) {
            assumeNonNull(mSectionHeaderModel);
            PropertyListModel<PropertyModel, PropertyKey> headerList =
                    mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY);
            PropertyModel headerModel = headerList.get(index);
            if (mTabToStreamMap.containsKey(index)
                    && mTabToStreamMap.get(index).supportsOptions()) {
                headerModel.set(
                        SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY,
                        ViewVisibility.INVISIBLE);
                headerModel.set(SectionHeaderProperties.OPTIONS_INDICATOR_IS_OPEN_KEY, false);
            }
            mOptionsCoordinator.ensureGone();
        }

        @Override
        public void onSectionHeaderReselected(int index) {
            Stream stream = assumeNonNull(mTabToStreamMap.get(index));
            if (!stream.supportsOptions()) return;

            assumeNonNull(mSectionHeaderModel);
            PropertyListModel<PropertyModel, PropertyKey> headerList =
                    mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY);
            PropertyModel headerModel = headerList.get(index);
            headerModel.set(
                    SectionHeaderProperties.OPTIONS_INDICATOR_IS_OPEN_KEY,
                    !headerModel.get(SectionHeaderProperties.OPTIONS_INDICATOR_IS_OPEN_KEY));
            // Reselected toggles the visibility of the options view.
            mOptionsCoordinator.toggleVisibility();
        }
    }

    /**
     * The {@link SignInPromo} for the Feed. TODO(huayinz): Update content and visibility through a
     * ModelChangeProcessor.
     */
    private class LegacyFeedSignInPromo extends SignInPromo {
        LegacyFeedSignInPromo(
                SigninManager signinManager, SyncPromoController syncPromoController) {
            super(signinManager, syncPromoController);
            maybeUpdateSignInPromo();
        }

        @Override
        protected void setVisibilityInternal(boolean visible) {
            if (isVisible() == visible) return;

            super.setVisibilityInternal(visible);
            mCoordinator.updateHeaderViews(visible ? mCoordinator.getSigninPromoView() : null);
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
                mSyncPromoController.setUpSyncPromoView(
                        mProfileDataCache,
                        mCoordinator
                                .getSigninPromoView()
                                .findViewById(R.id.signin_promo_view_container),
                        this::onDismissPromo);
            }
        }

        @Override
        public void onDismissPromo() {
            super.onDismissPromo();
            mCoordinator.updateHeaderViews(/* signinPromoView= */ null);
        }
    }

    /**
     * Wrapper class on top of {@link SigninPromoCoordinator} to also account for suggestions
     * available signal. TODO(crbug.com/448227402): remove this class once Seamless Sign-in is
     * launched.
     */
    private class FeedSigninPromo {
        private final SigninPromoCoordinator mSigninPromoCoordinator;
        private @Nullable View mPromoView;
        private boolean mCanShowPersonalizedSuggestions;
        private boolean mCanShowPromo;

        FeedSigninPromo(boolean canShowPersonalizedSuggestions) {
            mSigninPromoCoordinator =
                    new SigninPromoCoordinator(
                            mContext,
                            mProfile,
                            new NtpSigninPromoDelegate(
                                    mContext,
                                    mProfile,
                                    SigninAndHistorySyncActivityLauncherImpl.get(),
                                    this::onPromoStateChange));
            mCanShowPersonalizedSuggestions = canShowPersonalizedSuggestions;
            mCanShowPromo =
                    mSigninPromoCoordinator.canShowPromo() && mCanShowPersonalizedSuggestions;

            if (mCanShowPromo) {
                // The view is created lazily to avoid increasing the browser memory footprint by
                // keeping the view in memory even when it's never shown to the user.
                initializePromoView();
            }
        }

        boolean canShowPromo() {
            return mCanShowPromo;
        }

        @Nullable View getPromoView() {
            return mPromoView;
        }

        void setCanShowPersonalizedSuggestions(boolean canShow) {
            mCanShowPersonalizedSuggestions = canShow;
            onPromoStateChange();
        }

        void destroy() {
            mSigninPromoCoordinator.destroy();
            mCoordinator.updateHeaderViews(/* signinPromoView= */ null);
        }

        void onPromoStateChange() {
            boolean canShowPromo =
                    mSigninPromoCoordinator.canShowPromo() && mCanShowPersonalizedSuggestions;
            if (mCanShowPromo == canShowPromo) {
                return;
            }

            mCanShowPromo = canShowPromo;
            if (mPromoView == null && mCanShowPromo) {
                initializePromoView();
            }
            mCoordinator.updateHeaderViews(mCanShowPromo ? mPromoView : null);
        }

        private void initializePromoView() {
            mPromoView = mSigninPromoCoordinator.buildPromoView((ViewGroup) mCoordinator.getView());
            mSigninPromoCoordinator.setView(mPromoView);
        }
    }

    /** Internal implementation of Stream.StreamsMediator. */
    @VisibleForTesting
    public class StreamsMediatorImpl implements Stream.StreamsMediator {
        @Override
        public void switchToStreamKind(@StreamKind int streamKind) {
            int headerIndex = getTabIdForSection(streamKind);
            assert headerIndex != -1 : "Invalid header index for streamKind=" + streamKind;
            if (headerIndex != -1) {
                FeedSurfaceMediator.this.switchToStream(headerIndex);
            }
        }

        @Override
        public void refreshStream() {
            mCoordinator.nonSwipeRefresh();
        }
    }

    public static void setPrefForTest(
            PrefChangeRegistrar prefChangeRegistrar, PrefService prefService) {
        sTestPrefChangeRegistar = prefChangeRegistrar;
        sPrefServiceForTest = prefService;
    }

    private static @Nullable PrefChangeRegistrar sTestPrefChangeRegistar;
    private static @Nullable PrefService sPrefServiceForTest;
    private static final int SPAN_COUNT_SMALL_WIDTH = 1;
    private static final int SPAN_COUNT_LARGE_WIDTH = 2;
    private static final int SMALL_WIDTH_DP = 700;

    private final FeedSurfaceCoordinator mCoordinator;
    private final Context mContext;
    private final @Nullable SnapScrollHelper mSnapScrollHelper;
    private final Profile mProfile;
    private final PrefChangeRegistrar mPrefChangeRegistrar;
    private final SigninManager mSigninManager;
    private final TemplateUrlService mTemplateUrlService;
    private final @Nullable PropertyModel mSectionHeaderModel;
    private final FeedActionDelegate mActionDelegate;
    private final FeedOptionsCoordinator mOptionsCoordinator;

    // It is non-null for NTP on tablets.
    private @Nullable final UiConfig mUiConfig;
    private final DisplayStyleObserver mDisplayStyleObserver = this::onDisplayStyleChanged;
    private final ObservableSupplierImpl<Integer> mGetRestoringStateSupplier =
            new ObservableSupplierImpl<>(RestoringState.WAITING_TO_RESTORE);

    private RecyclerView.@Nullable OnScrollListener mStreamScrollListener;
    private final ObserverList<ScrollListener> mScrollListeners = new ObserverList<>();
    private final HasContentListener mHasContentListener;
    private @Nullable ContentChangedListener mStreamContentChangedListener;
    private @Nullable MemoryPressureCallback mMemoryPressureCallback;
    private @Nullable FeedSigninPromo mSigninPromo;
    private @Nullable SignInPromo mLegacySignInPromo;
    private final RecyclerViewAnimationFinishDetector mRecyclerViewAnimationFinishDetector =
            new RecyclerViewAnimationFinishDetector();

    private boolean mFeedEnabled;
    private boolean mTouchEnabled = true;
    private boolean mStreamContentChanged;
    private int mThumbnailWidth;
    private int mThumbnailHeight;
    private int mThumbnailScrollY;
    private int mRestoreTabId;
    private int mHeaderCount;

    /** The model representing feed-related cog menu items. */
    private @Nullable ModelList mFeedMenuModel;

    /** Whether the Feed content is loading. */
    private boolean mIsLoadingFeed;

    private @Nullable FeedScrollState mRestoreScrollState;
    private int mPositionToRestore = RecyclerView.NO_POSITION;

    // Track multiple streams when section header is supported.
    private final HashMap<Integer, Stream> mTabToStreamMap = new HashMap<>();
    // Track single main stream when section header is not supported.
    private @Nullable Stream mStreamHolder;
    private @Nullable Stream mCurrentStream;
    // Whether we're currently adding the streams. If this is true, streams should not be bound yet.
    // This avoids automatically binding the first stream when it's added.
    private boolean mSettingUpStreams;
    private final boolean mIsNewTabSearchEngineUrlAndroidEnabled;
    private boolean mIsPropertiesInitializedForStream;
    private @ClosedReason int mClosedReason = ClosedReason.SUSPEND_APP;
    private final boolean mIsNewTabPageCustomizationEnabled;

    /**
     * @param coordinator The {@link FeedSurfaceCoordinator} that interacts with this class.
     * @param context The current context.
     * @param snapScrollHelper The {@link SnapScrollHelper} that handles snap scrolling.
     * @param headerModel The {@link PropertyModel} that contains this mediator should work with.
     * @param openingTabId The {@link FeedSurfaceCoordinator.StreamTabId} the feed should open to.
     * @param optionsCoordinator The {@link FeedOptionsCoordinator} for the feed.
     * @param uiConfig The {@link UiConfig} for screen display.
     * @param profile The {@link Profile} for the current user.
     */
    FeedSurfaceMediator(
            FeedSurfaceCoordinator coordinator,
            Context context,
            @Nullable SnapScrollHelper snapScrollHelper,
            @Nullable PropertyModel headerModel,
            @SurfaceCoordinator.StreamTabId int openingTabId,
            FeedActionDelegate actionDelegate,
            FeedOptionsCoordinator optionsCoordinator,
            @Nullable UiConfig uiConfig,
            Profile profile) {
        mCoordinator = coordinator;
        mHasContentListener = coordinator;
        mContext = context;
        mSnapScrollHelper = snapScrollHelper;
        mSectionHeaderModel = headerModel;
        mProfile = profile;
        var signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        assertNonNull(signinManager);
        mSigninManager = signinManager;
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(mProfile);
        mActionDelegate = actionDelegate;
        mOptionsCoordinator = optionsCoordinator;
        mOptionsCoordinator.setOptionsListener(this);
        mIsNewTabSearchEngineUrlAndroidEnabled =
                DseNewTabUrlManager.isNewTabSearchEngineUrlAndroidEnabled();
        mIsNewTabPageCustomizationEnabled = ChromeFeatureList.sNewTabPageCustomization.isEnabled();
        mUiConfig = uiConfig;

        /*
         * When feature flag isNewTabSearchEngineUrlAndroidEnabled is enabled, the Feeds may be
         * hidden without showing its header. Therefore, FeedSurfaceMediator needs to observe
         * whether the DSE is changed and update Pref.ENABLE_SNIPPETS_BY_DSE even when Feeds isn't
         * visible.
         */
        mTemplateUrlService.addObserver(this);
        // It is possible that the default search engine has been changed before any NTP or
        // Start is showing, update the value of Pref.ENABLE_SNIPPETS_BY_DSE here. The
        // value should be updated before adding an observer to prevent an extra call of
        // updateContent().
        getPrefService()
                .setBoolean(
                        Pref.ENABLE_SNIPPETS_BY_DSE,
                        !mIsNewTabSearchEngineUrlAndroidEnabled
                                || mTemplateUrlService.isDefaultSearchEngineGoogle());

        if (sTestPrefChangeRegistar != null) {
            mPrefChangeRegistrar = sTestPrefChangeRegistar;
        } else {
            mPrefChangeRegistrar = PrefServiceUtil.createFor(profile);
        }
        mPrefChangeRegistrar.addObserver(Pref.ENABLE_SNIPPETS, this::updateContent);
        mPrefChangeRegistrar.addObserver(Pref.ENABLE_SNIPPETS_BY_DSE, this::updateContent);

        if (openingTabId == FeedSurfaceCoordinator.StreamTabId.DEFAULT) {
            mRestoreTabId = FeedFeatures.getFeedTabIdToRestore(mProfile);
        } else {
            mRestoreTabId = openingTabId;
        }

        // This works around the bug that the out-of-screen toolbar is not brought back together
        // with the new tab page view when it slides down. This is because the RecyclerView
        // animation may not finish when content changed event is triggered and thus the new tab
        // page layout view may still be partially off screen.
        mStreamContentChangedListener =
                contents ->
                        mRecyclerViewAnimationFinishDetector.runWhenAnimationComplete(
                                this::onContentsChanged);

        if (mUiConfig != null) {
            mUiConfig.addObserver(mDisplayStyleObserver);
            onDisplayStyleChanged(mUiConfig.getCurrentDisplayStyle());
        }

        initialize();
    }

    @Override
    public void onOptionChanged() {
        updateLayout(false);
    }

    private void updateLayout(boolean isSmallLayoutWidth) {
        ListLayoutHelper listLayoutHelper =
                mCoordinator.getHybridListRenderer().getListLayoutHelper();
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                || listLayoutHelper == null
                || mCurrentStream == null) {
            return;
        }
        int spanCount =
                shouldUseSingleSpan(isSmallLayoutWidth)
                        ? SPAN_COUNT_SMALL_WIDTH
                        : SPAN_COUNT_LARGE_WIDTH;
        boolean res = listLayoutHelper.setColumnCount(spanCount);
        assert res : "Failed to set column count on Feed";
    }

    private boolean shouldUseSingleSpan(boolean isSmallLayoutWidth) {
        assumeNonNull(mCurrentStream);
        boolean supportsOptions = mCurrentStream.supportsOptions();
        boolean isFollowingFeedSortDisabled =
                (!ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_FEED_SORT)
                        && mCurrentStream.getStreamKind() == StreamKind.FOLLOWING);
        boolean isFeedSortedBySite = false;
        if (supportsOptions) {
            @ContentOrder int selectedOption = mOptionsCoordinator.getSelectedOptionId();
            // Use single span count when showing following feed sorted by site.
            isFeedSortedBySite = (ContentOrder.GROUPED == selectedOption);
        }
        return isFollowingFeedSortDisabled || isSmallLayoutWidth || isFeedSortedBySite;
    }

    private void switchToStream(int headerIndex) {
        assert mSectionHeaderModel != null;
        PropertyListModel<PropertyModel, PropertyKey> headerList =
                mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY);
        mSectionHeaderModel.set(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY, headerIndex);

        // Proactively disable the unread content. Waiting for observers is too slow.
        headerList.get(headerIndex).set(SectionHeaderProperties.UNREAD_CONTENT_KEY, false);

        FeedFeatures.setLastSeenFeedTabId(mProfile, headerIndex);

        Stream newStream = assumeNonNull(mTabToStreamMap.get(headerIndex));
        if (newStream.supportsOptions()) {
            headerList
                    .get(headerIndex)
                    .set(
                            SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY,
                            ViewVisibility.VISIBLE);
        }
        if (!mSettingUpStreams) {
            logSwitchedFeeds(newStream);
            bindStream(newStream);
            if (newStream.getStreamKind() == StreamKind.FOLLOWING) {
                FeedFeatures.updateFollowingFeedSeen(mProfile);
            }
        }
    }

    /** Clears any dependencies. */
    void destroy() {
        destroyPropertiesForStream();
        mPrefChangeRegistrar.destroy();
        mTemplateUrlService.removeObserver(this);
        if (mUiConfig != null) {
            mUiConfig.removeObserver(mDisplayStyleObserver);
        }
    }

    public void destroyForTesting() {
        destroy();
    }

    private void initialize() {
        if (mSnapScrollHelper == null) return;

        // Listen for layout changes on the NewTabPageView itself to catch changes in scroll
        // position that are due to layout changes after e.g. device rotation. This contrasts with
        // regular scrolling, which is observed through an OnScrollListener.
        mCoordinator
                .getView()
                .addOnLayoutChangeListener(
                        (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                            mCoordinator.getView().postOnAnimation(mSnapScrollHelper::handleScroll);
                            float pixelToDp = mContext.getResources().getDisplayMetrics().density;
                            int widthDp = (int) ((right - left) / pixelToDp);
                            updateLayout(widthDp < SMALL_WIDTH_DP);
                        });
    }

    /**
     * Update the feed content based on whether it is enabled or disabled per enterprise policy.
     * When the feed is disabled, the feed content is completely gone.
     */
    void updateContent() {
        // See https://crbug.com/1498004.
        if (ApplicationStatus.isEveryActivityDestroyed()) return;

        mFeedEnabled = FeedFeatures.isFeedEnabled(mProfile);
        if (mFeedEnabled && (mStreamHolder != null || !mTabToStreamMap.isEmpty())) {
            return;
        }

        RecyclerView recyclerView = mCoordinator.getRecyclerView();
        if (mSnapScrollHelper != null && recyclerView != null) {
            mSnapScrollHelper.setView(recyclerView);
        }

        if (mFeedEnabled) {
            mIsLoadingFeed = true;
            mCoordinator.setupHeaders(/* feedEnabled= */ true);

            // Only set up stream if recycler view initiation did not fail.
            if (recyclerView != null) {
                initializePropertiesForStream();
            }
        } else {
            mCoordinator.setupHeaders(/* feedEnabled= */ false);
            destroyPropertiesForStream();
        }
    }

    /** Gets the current state, for restoring later. */
    String getSavedInstanceString() {
        FeedScrollState state = new FeedScrollState();
        if (mSectionHeaderModel != null) {
            int tabId = mSectionHeaderModel.get(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY);
            state.tabId = tabId;
        }
        LayoutManager layoutManager = null;
        if (mCoordinator.getRecyclerView() != null) {
            layoutManager = mCoordinator.getRecyclerView().getLayoutManager();
        }
        if (layoutManager != null) {
            ListLayoutHelper layoutHelper =
                    mCoordinator.getHybridListRenderer().getListLayoutHelper();
            assumeNonNull(layoutHelper);
            state.position = layoutHelper.findFirstVisibleItemPosition();
            state.lastPosition = layoutHelper.findLastVisibleItemPosition();
            if (state.position != RecyclerView.NO_POSITION) {
                View firstVisibleView = layoutManager.findViewByPosition(state.position);
                if (firstVisibleView != null) {
                    state.offset = firstVisibleView.getTop();
                }
            }
            if (mCurrentStream != null) {
                state.feedContentState = mCurrentStream.getContentState();
            }
        }
        return state.toJson();
    }

    /** Restores a previously saved state. */
    void restoreSavedInstanceState(@Nullable String json) {
        FeedScrollState state = FeedScrollState.fromJson(json);
        if (state == null) {
            mPositionToRestore = RecyclerView.NO_POSITION;
            mGetRestoringStateSupplier.set(RestoringState.NO_STATE_TO_RESTORE);
            return;
        }
        if (mSectionHeaderModel != null) {
            mRestoreTabId = state.tabId;
            mSectionHeaderModel.set(SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY, state.tabId);
        }
        mPositionToRestore = state.position;
        if (mCurrentStream == null) {
            mRestoreScrollState = state;
        } else {
            mCurrentStream.restoreSavedInstanceState(state);
        }
    }

    /**
     * Initialize properties for UI components in the {@link NewTabPage}. TODO(huayinz): Introduce a
     * Model for these properties.
     */
    private void initializePropertiesForStream() {
        assert !mSettingUpStreams;
        mSettingUpStreams = true;
        mPrefChangeRegistrar.addObserver(Pref.ARTICLES_LIST_VISIBLE, this::showOrHideFeed);

        boolean suggestionsVisible = isSuggestionsVisible();
        Stream mainStream =
                mCoordinator.createFeedStream(StreamKind.FOR_YOU, new StreamsMediatorImpl());

        mCoordinator.initializeBubbleTriggering();
        mSigninManager.getIdentityManager().addObserver(this);

        String headerText = getHeaderText(suggestionsVisible);
        if (mSectionHeaderModel != null) {
            addHeaderAndStream(headerText, mainStream);
            setHeaderIndicatorState(suggestionsVisible);

            mSectionHeaderModel.set(
                    SectionHeaderListProperties.ON_TAB_SELECTED_CALLBACK_KEY,
                    new FeedSurfaceHeaderSelectedCallback());

            if (!mIsNewTabPageCustomizationEnabled) {
                // Build menu after section enabled key is set.
                mFeedMenuModel = buildMenuItems();

                mSectionHeaderModel.set(
                        SectionHeaderListProperties.MENU_MODEL_LIST_KEY, mFeedMenuModel);
                mSectionHeaderModel.set(SectionHeaderListProperties.MENU_DELEGATE_KEY, this);
            }

            setUpWebFeedTab();

            // Set the current tab index to what restoreSavedInstanceState had.
            if (mTabToStreamMap.size() <= mRestoreTabId) mRestoreTabId = 0;
            mSectionHeaderModel.set(
                    SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY, mRestoreTabId);

            if (mSectionHeaderModel.get(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY)) {
                bindStream(
                        mTabToStreamMap.get(
                                mSectionHeaderModel.get(
                                        SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY)));
            } else {
                unbindStream();
            }
        } else {
            mCoordinator.updateHeaderText(headerText);
            mStreamHolder = mainStream;
            bindStream(mStreamHolder);
        }

        mSettingUpStreams = false;

        mStreamScrollListener =
                new RecyclerView.OnScrollListener() {
                    private final RecyclerViewAnimationFinishDetector mAnimationFinishDetector =
                            new RecyclerViewAnimationFinishDetector();

                    @Override
                    public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                        for (ScrollListener listener : mScrollListeners) {
                            listener.onScrollStateChanged(newState);
                        }
                    }

                    @Override
                    public void onScrolled(RecyclerView v, int dx, int dy) {
                        final Runnable callback =
                                () -> {
                                    if (mSnapScrollHelper != null) {
                                        mSnapScrollHelper.handleScroll();
                                    }
                                    for (ScrollListener listener : mScrollListeners) {
                                        listener.onScrolled(dx, dy);
                                    }
                                    // Null if the stream has not been binded yet.
                                    if (GestureNavigationUtils.shouldAnimateBackForwardTransitions()
                                            && mCoordinator.getHybridListRenderer() != null
                                            && mCoordinator
                                                            .getHybridListRenderer()
                                                            .getListLayoutHelper()
                                                    != null
                                            && mPositionToRestore != RecyclerView.NO_POSITION
                                            && Objects.equals(
                                                    mGetRestoringStateSupplier.get(),
                                                    RestoringState.WAITING_TO_RESTORE)) {
                                        final boolean restored =
                                                mCoordinator
                                                                .getHybridListRenderer()
                                                                .getListLayoutHelper()
                                                                .findFirstVisibleItemPosition()
                                                        >= mPositionToRestore;
                                        if (restored) {
                                            mPositionToRestore = RecyclerView.NO_POSITION;
                                            final var originalAnimator =
                                                    mCoordinator
                                                            .getRecyclerView()
                                                            .getItemAnimator();
                                            mCoordinator
                                                    .getRecyclerView()
                                                    .setItemAnimator(
                                                            new ItemAnimatorWithoutAnimation());
                                            final Runnable onComplete =
                                                    () -> {
                                                        mGetRestoringStateSupplier.set(
                                                                RestoringState.RESTORED);
                                                        mCoordinator
                                                                .getRecyclerView()
                                                                .setItemAnimator(originalAnimator);
                                                        mAnimationFinishDetector
                                                                .runWhenAnimationComplete(null);
                                                    };
                                            mAnimationFinishDetector.runWhenAnimationComplete(
                                                    onComplete);
                                        }
                                    }
                                };
                        mCoordinator.getView().postOnAnimation(callback);
                    }
                };
        var view = mCoordinator.getRecyclerView();
        view.addOnScrollListener(mStreamScrollListener);

        initStreamHeaderViews();

        mMemoryPressureCallback = pressure -> view.getRecycledViewPool().clear();
        MemoryPressureListener.addCallback(mMemoryPressureCallback);

        mIsPropertiesInitializedForStream = true;
    }

    void addScrollListener(ScrollListener listener) {
        mScrollListeners.addObserver(listener);
    }

    void removeScrollListener(ScrollListener listener) {
        mScrollListeners.removeObserver(listener);
    }

    private void addHeaderAndStream(@Nullable String headerText, Stream stream) {
        assumeNonNull(mSectionHeaderModel);
        int tabId = mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY).size();
        mTabToStreamMap.put(tabId, stream);

        PropertyModel headerModel = SectionHeaderProperties.createSectionHeader(headerText);
        ViewVisibility indicatorVisibility;
        // Keeping the indicator in place for the "Following" header, so it allows a fixed width of
        // the "Following" header.
        if (stream.supportsOptions() || stream.getStreamKind() == StreamKind.FOLLOWING) {
            indicatorVisibility = ViewVisibility.INVISIBLE;
        } else {
            indicatorVisibility = ViewVisibility.GONE;
        }
        headerModel.set(
                SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY, indicatorVisibility);
        headerModel.set(SectionHeaderProperties.OPTIONS_INDICATOR_IS_OPEN_KEY, false);
        mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY).add(headerModel);

        // Update UNREAD_CONTENT_KEY and HEADER_ACCESSIBILITY_TEXT_KEY now, and any time
        // hasUnreadContent() changes.
        Callback<Boolean> callback =
                hasUnreadContent -> {
                    headerModel.set(SectionHeaderProperties.UNREAD_CONTENT_KEY, hasUnreadContent);
                    mHasContentListener.hasContentChanged(stream.getStreamKind(), hasUnreadContent);
                };
        Boolean hasUnreadContent = stream.hasUnreadContent().addObserver(callback);
        callback.onResult(Boolean.TRUE.equals(hasUnreadContent));
    }

    private int getTabIdForSection(@StreamKind int streamKind) {
        for (int tabId : mTabToStreamMap.keySet()) {
            Stream stream = assumeNonNull(mTabToStreamMap.get(tabId));
            if (stream.getStreamKind() == streamKind) {
                return tabId;
            }
        }
        return -1;
    }

    /** Adds WebFeed tab if we need it. */
    private void setUpWebFeedTab() {
        // Skip if the for-you tab hasn't been added yet.
        if (getTabIdForSection(StreamKind.FOR_YOU) == -1) {
            return;
        }
        int tabId = getTabIdForSection(StreamKind.FOLLOWING);
        boolean hasWebFeedTab = tabId != -1;
        boolean shouldHaveWebFeedTab = FeedFeatures.isWebFeedUIEnabled(mProfile);
        if (hasWebFeedTab == shouldHaveWebFeedTab) return;
        if (shouldHaveWebFeedTab) {
            addHeaderAndStream(
                    mContext.getString(R.string.ntp_following),
                    mCoordinator.createFeedStream(StreamKind.FOLLOWING, new StreamsMediatorImpl()));
            if (FeedFeatures.shouldUseNewIndicator(mProfile)) {
                assumeNonNull(mSectionHeaderModel);
                PropertyModel followingHeaderModel =
                        mSectionHeaderModel
                                .get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                                .get(getTabIdForSection(StreamKind.FOLLOWING));
                followingHeaderModel.set(
                        SectionHeaderProperties.BADGE_TEXT_KEY,
                        mContext.getString(R.string.ntp_new));

                // Set up a content changed listener on the main feed to start animation
                // after main feed loads more than 1 feed card.
                Stream mainFeedStream = mTabToStreamMap.get(getTabIdForSection(StreamKind.FOR_YOU));
                assumeNonNull(mainFeedStream);
                mainFeedStream.addOnContentChangedListener(
                        new ContentChangedListener() {
                            @Override
                            public void onContentChanged(
                                    @Nullable List<FeedListContentManager.FeedContent>
                                            feedContents) {
                                if (feedContents != null
                                        && feedContents.size() > mHeaderCount + 1) {
                                    followingHeaderModel.set(
                                            SectionHeaderProperties.ANIMATION_START_KEY, true);
                                    FeedFeatures.updateNewIndicatorTimestamp(mProfile);
                                    mainFeedStream.removeOnContentChangedListener(this);
                                }
                            }
                        });
            }
        }
    }

    /**
     * Binds a stream to the {@link FeedListContentManager}. Unbinds currently active stream if
     * different from new stream. Once bound, the stream can add/remove contents.
     */
    @VisibleForTesting
    void bindStream(@Nullable Stream stream) {
        if (mCurrentStream == stream) return;
        if (mCurrentStream != null) {
            unbindStream(/* shouldPlaceSpacer= */ true, /* switchingStream= */ true);
        }
        // Don't bind before the coordinator is active, or when the feed should not show.
        if (!mCoordinator.isActive()
                || (mSectionHeaderModel != null
                        && !mSectionHeaderModel.get(
                                SectionHeaderListProperties.IS_SECTION_ENABLED_KEY))
                || !isSuggestionsVisible()) {
            return;
        }
        mCurrentStream = stream;
        updateLayout(false);
        assumeNonNull(mCurrentStream);
        assumeNonNull(mStreamContentChangedListener);
        mCurrentStream.addOnContentChangedListener(mStreamContentChangedListener);

        mGetRestoringStateSupplier.set(RestoringState.WAITING_TO_RESTORE);
        FeedReliabilityLogger reliabilityLogger = mCoordinator.getReliabilityLogger();
        mCurrentStream.bind(
                mCoordinator.getRecyclerView(),
                mCoordinator.getContentManager(),
                mRestoreScrollState,
                mCoordinator.getSurfaceScope(),
                mCoordinator.getHybridListRenderer(),
                reliabilityLogger,
                mHeaderCount);
        mRestoreScrollState = null;
        mCoordinator.getHybridListRenderer().onSurfaceOpened();
    }

    void onContentsChanged() {
        if (mSnapScrollHelper != null) mSnapScrollHelper.resetSearchBoxOnScroll(true);

        mActionDelegate.onContentsChanged();

        mIsLoadingFeed = false;
        mStreamContentChanged = true;
    }

    public boolean isLoadingFeed() {
        return mIsLoadingFeed;
    }

    public List<String> getFeedUrls() {
        return (mCurrentStream != null) ? mCurrentStream.getFeedUrls() : new ArrayList<String>();
    }

    /** Unbinds the stream and clear all the stream's contents. */
    private void unbindStream() {
        unbindStream(false, false);
    }

    /** Unbinds the stream with option for stream to put a placeholder for its contents. */
    private void unbindStream(boolean shouldPlaceSpacer, boolean switchingStream) {
        if (mCurrentStream == null) return;
        mClosedReason = mCurrentStream.getClosedReason();
        mCoordinator.getHybridListRenderer().onSurfaceClosed();
        mCurrentStream.unbind(shouldPlaceSpacer, switchingStream);
        assumeNonNull(mStreamContentChangedListener);
        mCurrentStream.removeOnContentChangedListener(mStreamContentChangedListener);
        mCurrentStream = null;
    }

    void onSurfaceOpened() {
        rebindStream();
    }

    void onSurfaceClosed() {
        unbindStream();
    }

    /**
     * @return The stream that represents the 1st tab.
     */
    boolean hasStreams() {
        return mStreamHolder != null || !mTabToStreamMap.isEmpty();
    }

    long getLastFetchTimeMsForCurrentStream() {
        if (mCurrentStream == null) return 0;
        return mCurrentStream.getLastFetchTimeMs();
    }

    @Nullable Stream getCurrentStreamForTesting() {
        return mCurrentStream;
    }

    private void rebindStream() {
        // If a stream is already bound, then do nothing.
        if (mCurrentStream != null) return;
        // Find the stream that should be bound and bind it. If no stream matches, then we haven't
        // fully set up yet. This will be taken care of by setup.
        Stream stream = null;
        if (mSectionHeaderModel != null) {
            stream =
                    mTabToStreamMap.get(
                            mSectionHeaderModel.get(
                                    SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));

        } else if (mStreamHolder != null) {
            stream = mStreamHolder;
        }
        if (stream != null) {
            bindStream(stream);
        }
    }

    /**
     * Notifies a bound stream of new header count number.
     *
     * @param newHeaderCount Number of headers in the {@link RecyclerView}.
     */
    void notifyHeadersChanged(int newHeaderCount) {
        mHeaderCount = newHeaderCount;
        if (mCurrentStream != null) {
            mCurrentStream.notifyNewHeaderCount(newHeaderCount);
        }
    }

    private void initStreamHeaderViews() {
        boolean signInPromoVisible = shouldShowSigninPromo();
        if (signInPromoVisible && mSigninPromo != null) {
            mCoordinator.updateHeaderViews(mSigninPromo.getPromoView());
        } else if (signInPromoVisible && mLegacySignInPromo != null) {
            mCoordinator.updateHeaderViews(mCoordinator.getSigninPromoView());
        } else {
            mCoordinator.updateHeaderViews(/* signinPromoView= */ null);
        }
    }

    /**
     * Determines whether a signin promo should be shown.
     *
     * @return Whether the SignPromo should be visible.
     */
    private boolean shouldShowSigninPromo() {
        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            return false;
        }
        // TODO(crbug.com/352735671): Move SignInPromo.shouldCreatePromo inside FeedSigninPromo
        //  after phase 2 follow-up launch.§
        boolean shouldCreatePromo = SignInPromo.shouldCreatePromo();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)) {
            if (!shouldCreatePromo) {
                return false;
            }
            if (mSigninPromo == null) {
                mSigninPromo = new FeedSigninPromo(isSuggestionsVisible());
            }
            return mSigninPromo.canShowPromo();
        } else {
            AccountPickerBottomSheetStrings bottomSheetStrings =
                    new AccountPickerBottomSheetStrings.Builder(
                                    mContext.getString(
                                            R.string.signin_account_picker_bottom_sheet_title))
                            .build();
            SyncPromoController promoController =
                    new SyncPromoController(
                            mProfile,
                            bottomSheetStrings,
                            SigninAccessPoint.NTP_FEED_TOP_PROMO,
                            SigninAndHistorySyncActivityLauncherImpl.get());
            if (!shouldCreatePromo || !promoController.canShowSyncPromo()) {
                return false;
            }
            if (mLegacySignInPromo == null) {
                mLegacySignInPromo = new LegacyFeedSignInPromo(mSigninManager, promoController);
                mLegacySignInPromo.setCanShowPersonalizedSuggestions(isSuggestionsVisible());
            }
            return mLegacySignInPromo.isVisible();
        }
    }

    /** Clear any dependencies related to the {@link Stream}. */
    @VisibleForTesting
    void destroyPropertiesForStream() {
        if (mStreamScrollListener != null) {
            mCoordinator.getRecyclerView().removeOnScrollListener(mStreamScrollListener);
            mStreamScrollListener = null;
        }

        if (mMemoryPressureCallback != null) {
            MemoryPressureListener.removeCallback(mMemoryPressureCallback);
            mMemoryPressureCallback = null;
        }

        if (mLegacySignInPromo != null) {
            mLegacySignInPromo.destroy();
            mLegacySignInPromo = null;
        }
        if (mSigninPromo != null) {
            mSigninPromo.destroy();
            mSigninPromo = null;
        }

        if (mSectionHeaderModel != null) {
            if (!mTabToStreamMap.isEmpty()) {
                mSectionHeaderModel.get(SectionHeaderListProperties.SECTION_HEADERS_KEY).clear();
                assumeNonNull(mStreamContentChangedListener);
                for (Stream s : mTabToStreamMap.values()) {
                    s.removeOnContentChangedListener(mStreamContentChangedListener);
                    s.destroy();
                }
                mTabToStreamMap.clear();
            }
        } else {
            if (mStreamHolder != null) {
                assumeNonNull(mStreamContentChangedListener);
                mStreamHolder.removeOnContentChangedListener(mStreamContentChangedListener);
                mStreamHolder = null;
            }
        }
        mStreamContentChangedListener = null;
        unbindStream();

        mPrefChangeRegistrar.removeObserver(Pref.ARTICLES_LIST_VISIBLE);
        mSigninManager.getIdentityManager().removeObserver(this);

        mIsPropertiesInitializedForStream = false;

        if (mCoordinator.getSurfaceScope() != null) {
            mCoordinator.getSurfaceScope().getLaunchReliabilityLogger().cancelPendingEvents();
        }
    }

    private void setHeaderIndicatorState(boolean suggestionsVisible) {
        assert mSectionHeaderModel != null;
        boolean isSignedIn = FeedServiceBridge.isSignedIn();
        boolean isTabMode =
                isSignedIn && FeedFeatures.isWebFeedUIEnabled(mProfile) && suggestionsVisible;
        // If we're in tab mode now, make sure webfeed tab is set up.
        if (isTabMode) {
            setUpWebFeedTab();
        }
        mSectionHeaderModel.set(SectionHeaderListProperties.IS_TAB_MODE_KEY, isTabMode);

        // If not in tab mode, make sure we are on the for-you feed.
        if (!isTabMode) {
            mSectionHeaderModel.set(
                    SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY,
                    PRIMARY_FEED_HEADER_POSITION);
        }

        boolean isGoogleSearchEngine = mTemplateUrlService.isDefaultSearchEngineGoogle();
        // When Google is not the default search engine, we need to show the Logo.
        mSectionHeaderModel.set(
                SectionHeaderListProperties.IS_LOGO_KEY,
                !isGoogleSearchEngine && isSignedIn && suggestionsVisible);
        ViewVisibility indicatorState;
        if (!isTabMode) {
            // Gone when the following/for you tab switcher header is not shown
            indicatorState = ViewVisibility.GONE;
        } else if (!isGoogleSearchEngine) {
            // Visible when Google is not the search engine (show logo).
            indicatorState = ViewVisibility.VISIBLE;
        } else {
            // Invisible when we have centered text (signed in and not shown). This
            // counterbalances the gear icon so text is properly centered.
            indicatorState = ViewVisibility.INVISIBLE;
        }
        mSectionHeaderModel.set(
                SectionHeaderListProperties.INDICATOR_VIEW_VISIBILITY_KEY, indicatorState);

        // Make sure to collapse option panel if not shown.
        if (!suggestionsVisible) {
            mOptionsCoordinator.ensureGone();
        }

        // Set enabled last because it makes the animation smoother.
        mSectionHeaderModel.set(
                SectionHeaderListProperties.IS_SECTION_ENABLED_KEY, suggestionsVisible);
    }

    /**
     * Show or hide the feed. When the feed is hidden, the discover off message will be shown.
     *
     * <p>Called when a settings change or update to this/another NTP caused the feed to show/hide.
     */
    void showOrHideFeed() {
        // It is possible that showOrHideFeed() is called when the surface which contains the
        // Feeds isn't visible or headers of streams haven't been added, returns here.
        // See https://crbug.com/1485070 and https://crbug.com/1488210.
        // TODO(crbug.com/40934702): Figure out the root cause of setting
        // SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY to -1 and fix it.
        if (!mIsPropertiesInitializedForStream
                || (mSectionHeaderModel != null
                        && mSectionHeaderModel.get(
                                        SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY)
                                < 0)) {
            return;
        }

        boolean suggestionsVisible = isSuggestionsVisible();

        if (mLegacySignInPromo != null) {
            mLegacySignInPromo.setCanShowPersonalizedSuggestions(suggestionsVisible);
        }
        if (mSigninPromo != null) {
            mSigninPromo.setCanShowPersonalizedSuggestions(suggestionsVisible);
        }
        if (suggestionsVisible) {
            assumeNonNull(mCoordinator.getSurfaceLifecycleManager()).show();
        }
        mStreamContentChanged = true;

        String headerText = getHeaderText(suggestionsVisible);
        if (mSectionHeaderModel != null) {
            mSectionHeaderModel
                    .get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                    .get(PRIMARY_FEED_HEADER_POSITION)
                    .set(SectionHeaderProperties.HEADER_TEXT_KEY, headerText);

            setHeaderIndicatorState(suggestionsVisible);

            if (!mIsNewTabPageCustomizationEnabled) {
                // Update toggleswitch item, which is last item in list.
                mSectionHeaderModel.set(
                        SectionHeaderListProperties.MENU_MODEL_LIST_KEY, buildMenuItems());
            }

            PropertyModel currentStreamHeaderModel =
                    mSectionHeaderModel
                            .get(SectionHeaderListProperties.SECTION_HEADERS_KEY)
                            .get(
                                    mSectionHeaderModel.get(
                                            SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
            Stream currentStream =
                    mTabToStreamMap.get(
                            mSectionHeaderModel.get(
                                    SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY));
            if (currentStream != null && currentStream.supportsOptions()) {
                currentStreamHeaderModel.set(
                        SectionHeaderProperties.OPTIONS_INDICATOR_VISIBILITY_KEY,
                        suggestionsVisible ? ViewVisibility.VISIBLE : ViewVisibility.INVISIBLE);
                if (!suggestionsVisible) {
                    currentStreamHeaderModel.set(
                            SectionHeaderProperties.OPTIONS_INDICATOR_IS_OPEN_KEY, false);
                }
            }
        } else {
            mCoordinator.updateHeaderText(headerText);
        }

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
        assert mSectionHeaderModel != null;
        boolean isExpanded =
                !mSectionHeaderModel.get(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY);

        // Record in prefs and UMA.
        // Model and stream visibility set in {@link #showOrHideFeed}
        // which is called by the prefService observer.
        getPrefService().setBoolean(Pref.ARTICLES_LIST_VISIBLE, isExpanded);

        if (!mIsNewTabPageCustomizationEnabled) {
            FeedUma.recordArticlesListVisible(isExpanded);
        }

        int streamType =
                assumeNonNull(
                                mTabToStreamMap.get(
                                        mSectionHeaderModel.get(
                                                SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY)))
                        .getStreamKind();
        FeedServiceBridge.reportOtherUserAction(
                streamType,
                isExpanded
                        ? FeedUserActionType.TAPPED_TURN_ON
                        : FeedUserActionType.TAPPED_TURN_OFF);
    }

    /** Returns the feed header text. */
    private @Nullable String getHeaderText(boolean isExpanded) {
        if (isExpanded && ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_HEADER_REMOVAL)) {
            String treatment =
                    ChromeFeatureList.getFieldTrialParamByFeature(
                            ChromeFeatureList.FEED_HEADER_REMOVAL, "treatment");
            // Returns null to indicate that no feed header is shown.
            if (!treatment.equals("label")) return null;
        }

        Resources res = mContext.getResources();
        if (mSectionHeaderModel != null
                && WebFeedBridge.isWebFeedEnabled()
                && FeedServiceBridge.isSignedIn()
                && isExpanded) {
            return res.getString(R.string.ntp_discover_on);
        } else if (mTemplateUrlService.isDefaultSearchEngineGoogle()) {
            return isExpanded
                    ? res.getString(R.string.ntp_discover_on)
                    : res.getString(R.string.ntp_discover_off);
        }
        return isExpanded
                ? res.getString(R.string.ntp_discover_on_branded)
                : res.getString(R.string.ntp_discover_off_branded);
    }

    private ModelList buildMenuItems() {
        assert mSectionHeaderModel != null;

        ModelList itemList = new ModelList();
        int iconId = 0;

        if (FeedServiceBridge.isSignedIn()) {
            if (WebFeedBridge.isWebFeedEnabled()) {
                itemList.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.ntp_manage_feed)
                                .withMenuId(R.id.ntp_feed_header_menu_item_manage)
                                .withStartIconRes(iconId)
                                .build());
            } else {
                itemList.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.ntp_manage_my_activity)
                                .withMenuId(R.id.ntp_feed_header_menu_item_activity)
                                .withStartIconRes(iconId)
                                .build());
                itemList.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.ntp_manage_interests)
                                .withMenuId(R.id.ntp_feed_header_menu_item_interest)
                                .withStartIconRes(iconId)
                                .build());
            }
        }
        itemList.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.learn_more)
                        .withMenuId(R.id.ntp_feed_header_menu_item_learn)
                        .withStartIconRes(iconId)
                        .build());
        itemList.add(
                getMenuToggleSwitch(
                        mSectionHeaderModel.get(SectionHeaderListProperties.IS_SECTION_ENABLED_KEY),
                        iconId));
        return itemList;
    }

    /**
     * Returns the menu list item that represents turning the feed on/off.
     *
     * @param isEnabled Whether the feed section is currently enabled.
     * @param iconId IconId for the list item if any.
     */
    private MVCListAdapter.ListItem getMenuToggleSwitch(boolean isEnabled, int iconId) {
        return new ListItemBuilder()
                .withTitleRes(isEnabled ? R.string.ntp_turn_off_feed : R.string.ntp_turn_on_feed)
                .withMenuId(R.id.ntp_feed_header_menu_item_toggle_switch)
                .withStartIconRes(iconId)
                .build();
    }

    /** Whether a new thumbnail should be captured. */
    boolean shouldCaptureThumbnail() {
        return mStreamContentChanged
                || mCoordinator.getView().getWidth() != mThumbnailWidth
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
     * @return Whether the touch events are enabled. TODO(huayinz): Move this method to a Model once
     *     a Model is introduced.
     */
    boolean getTouchEnabled() {
        return mTouchEnabled;
    }

    // TODO(carlosk): replace with FeedFeatures.getPrefService().
    private PrefService getPrefService() {
        if (sPrefServiceForTest != null) return sPrefServiceForTest;
        return UserPrefs.get(mProfile);
    }

    // TouchEnabledDelegate interface.
    @Override
    public void setTouchEnabled(boolean enabled) {
        mTouchEnabled = enabled;
    }

    // ScrollDelegate interface.
    @Override
    public boolean isScrollViewInitialized() {
        RecyclerView recyclerView = mCoordinator.getRecyclerView();
        return recyclerView != null && (!mFeedEnabled || recyclerView.getHeight() > 0);
    }

    @Override
    public int getVerticalScrollOffset() {
        // This method returns a valid vertical scroll offset only when the first header view in the
        // Stream is visible.
        if (!isScrollViewInitialized()) return 0;

        if (!isChildVisibleAtPosition(0)) {
            return Integer.MIN_VALUE;
        }

        LayoutManager layoutManager = mCoordinator.getRecyclerView().getLayoutManager();
        if (layoutManager == null) {
            return Integer.MIN_VALUE;
        }

        View view = layoutManager.findViewByPosition(0);
        if (view == null) {
            return Integer.MIN_VALUE;
        }

        return -view.getTop();
    }

    @Override
    public boolean isChildVisibleAtPosition(int position) {
        if (!isScrollViewInitialized()) return false;

        // When feed is disabled on tablet, the existing implementation of ListLayoutHelper for
        // staggered layout doesn't return the first and last visible item positions correctly. To
        // work around this, we check for the header explicitly.
        if (!mFeedEnabled && position == 0) {
            return mHeaderCount > 0;
        }

        ListLayoutHelper layoutHelper = mCoordinator.getHybridListRenderer().getListLayoutHelper();
        if (layoutHelper == null) {
            return false;
        }

        int firstItemPosition = layoutHelper.findFirstVisibleItemPosition();
        int lastItemPosition = layoutHelper.findLastVisibleItemPosition();
        if (firstItemPosition == RecyclerView.NO_POSITION
                || lastItemPosition == RecyclerView.NO_POSITION) {
            return false;
        }

        return firstItemPosition <= position && position <= lastItemPosition;
    }

    @Override
    public void snapScroll() {
        if (mSnapScrollHelper == null) return;
        if (!isScrollViewInitialized()) return;

        int initialScroll = getVerticalScrollOffset();
        int scrollTo = mSnapScrollHelper.calculateSnapPosition(initialScroll);

        // Calculating the snap position should be idempotent.
        assert scrollTo == mSnapScrollHelper.calculateSnapPosition(scrollTo);

        mCoordinator.getRecyclerView().smoothScrollBy(0, scrollTo - initialScroll);
    }

    /**
     * Scrolls the page to show the view at the given {@code viewPosition} if not already visible.
     *
     * @param viewPosition The position of the view that should be visible or scrolled to.
     */
    void scrollToViewIfNecessary(int viewPosition) {
        if (!isScrollViewInitialized()) return;
        if (!isChildVisibleAtPosition(viewPosition)) {
            mCoordinator.getRecyclerView().scrollToPosition(viewPosition);
        }
    }

    @Override
    public void onTemplateURLServiceChanged() {
        if (mIsNewTabSearchEngineUrlAndroidEnabled) {
            getPrefService()
                    .setBoolean(
                            Pref.ENABLE_SNIPPETS_BY_DSE,
                            mTemplateUrlService.isDefaultSearchEngineGoogle());
            return;
        }
        showOrHideFeed();
    }

    @Override
    public void onItemSelected(PropertyModel item, View view) {
        assert mSectionHeaderModel != null;
        int itemId = item.get(ListMenuItemProperties.MENU_ITEM_ID);
        int feedType =
                assumeNonNull(
                                mTabToStreamMap.get(
                                        mSectionHeaderModel.get(
                                                SectionHeaderListProperties.CURRENT_TAB_INDEX_KEY)))
                        .getStreamKind();
        if (itemId == R.id.ntp_feed_header_menu_item_manage) {
            Intent intent = new Intent(mContext, FeedManagementActivity.class);
            intent.putExtra(FeedManagementActivity.INITIATING_STREAM_TYPE_EXTRA, feedType);
            FeedServiceBridge.reportOtherUserAction(feedType, FeedUserActionType.TAPPED_MANAGE);
            mContext.startActivity(intent);
        } else if (itemId == R.id.ntp_feed_header_menu_item_activity) {
            mActionDelegate.openUrl(
                    WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams("https://myactivity.google.com/myactivity?product=50"));
            FeedServiceBridge.reportOtherUserAction(
                    feedType, FeedUserActionType.TAPPED_MANAGE_ACTIVITY);
        } else if (itemId == R.id.ntp_feed_header_menu_item_interest) {
            mActionDelegate.openUrl(
                    WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams("https://www.google.com/preferences/interests"));
            FeedServiceBridge.reportOtherUserAction(
                    feedType, FeedUserActionType.TAPPED_MANAGE_INTERESTS);
        } else if (itemId == R.id.ntp_feed_header_menu_item_reactions) {
            mActionDelegate.openUrl(
                    WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams("https://www.google.com/search/contributions/reactions"));
            FeedServiceBridge.reportOtherUserAction(
                    feedType, FeedUserActionType.TAPPED_MANAGE_REACTIONS);
        } else if (itemId == R.id.ntp_feed_header_menu_item_learn) {
            mActionDelegate.openHelpPage();
            FeedServiceBridge.reportOtherUserAction(feedType, FeedUserActionType.TAPPED_LEARN_MORE);
        } else if (itemId == R.id.ntp_feed_header_menu_item_toggle_switch) {
            onSectionHeaderToggled();
        } else {
            assert false
                    : String.format(
                            Locale.ENGLISH,
                            "Cannot handle action for item in the menu with id %d",
                            itemId);
        }
    }

    // IdentityManager.Observer interface.

    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        showOrHideFeed();
    }

    /**
     * The state of whether the feed stream is being restored.
     *
     * @return The restoring state {@link RestoringState}.
     */
    public ObservableSupplier<Integer> getRestoringStateSupplier() {
        return mGetRestoringStateSupplier;
    }

    public @Nullable SignInPromo getSignInPromoForTesting() {
        return mLegacySignInPromo;
    }

    void manualRefresh(Callback<Boolean> callback) {
        if (mCurrentStream != null) {
            mCurrentStream.triggerRefresh(callback);
        } else {
            callback.onResult(false);
        }
    }

    // Detects animation finishes in RecyclerView.
    // https://stackoverflow.com/questions/33710605/detect-animation-finish-in-androids-recyclerview
    private class RecyclerViewAnimationFinishDetector
            implements RecyclerView.ItemAnimator.ItemAnimatorFinishedListener {
        private @Nullable Runnable mFinishedCallback;

        /**
         * Asynchronously waits for the animation to finish. If there's already a callback waiting,
         * this replaces the existing callback.
         *
         * @param finishedCallback Callback to invoke when the animation finishes.
         */
        public void runWhenAnimationComplete(@Nullable Runnable finishedCallback) {
            if (mCoordinator.getRecyclerView() == null) {
                return;
            }
            mFinishedCallback = finishedCallback;

            // The RecyclerView has not started animating yet, so post a message to the
            // message queue that will be run after the RecyclerView has started animating.
            new Handler().post(this::checkFinish);
        }

        private void checkFinish() {
            RecyclerView recyclerView = mCoordinator.getRecyclerView();

            if (recyclerView != null && recyclerView.isAnimating()) {
                // The RecyclerView is still animating, try again when the animation has finished.
                assumeNonNull(recyclerView.getItemAnimator()).isRunning(this);
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
            new Handler().post(this::checkFinish);
        }
    }

    // Force RecyclerView to skip all animations.
    private static class ItemAnimatorWithoutAnimation extends RecyclerView.ItemAnimator {

        @Override
        public boolean animateDisappearance(
                RecyclerView.ViewHolder viewHolder,
                ItemHolderInfo itemHolderInfo,
                @Nullable ItemHolderInfo itemHolderInfo1) {
            return false;
        }

        @Override
        public boolean animateAppearance(
                RecyclerView.ViewHolder viewHolder,
                @Nullable ItemHolderInfo itemHolderInfo,
                ItemHolderInfo itemHolderInfo1) {
            return false;
        }

        @Override
        public boolean animatePersistence(
                RecyclerView.ViewHolder viewHolder,
                ItemHolderInfo itemHolderInfo,
                ItemHolderInfo itemHolderInfo1) {
            return false;
        }

        @Override
        public boolean animateChange(
                RecyclerView.ViewHolder viewHolder,
                RecyclerView.ViewHolder viewHolder1,
                ItemHolderInfo itemHolderInfo,
                ItemHolderInfo itemHolderInfo1) {
            return false;
        }

        @Override
        public void runPendingAnimations() {}

        @Override
        public void endAnimation(RecyclerView.ViewHolder viewHolder) {}

        @Override
        public void endAnimations() {}

        @Override
        public boolean isRunning() {
            return false;
        }
    }

    private @StreamType int getStreamType(Stream stream) {
        switch (stream.getStreamKind()) {
            case StreamKind.FOR_YOU:
                return StreamType.FOR_YOU;
            case StreamKind.FOLLOWING:
                return StreamType.WEB_FEED;
            default:
                return StreamType.UNSPECIFIED;
        }
    }

    private void logSwitchedFeeds(Stream switchedToStream) {
        // Log the end of an ongoing launch and the beginning of a new one.
        FeedReliabilityLogger reliabilityLogger = mCoordinator.getReliabilityLogger();
        if (reliabilityLogger != null) {
            reliabilityLogger.onSwitchStream(getStreamType(switchedToStream));
        }
    }

    public boolean isSuggestionsVisible() {
        return getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE);
    }

    public boolean isFeedEnabled() {
        return mFeedEnabled;
    }

    OnSectionHeaderSelectedListener getOrCreateSectionHeaderListenerForTesting() {
        OnSectionHeaderSelectedListener listener =
                assumeNonNull(mSectionHeaderModel)
                        .get(SectionHeaderListProperties.ON_TAB_SELECTED_CALLBACK_KEY);
        if (listener == null) {
            listener = new FeedSurfaceHeaderSelectedCallback();
        }
        return listener;
    }

    void setStreamForTesting(int key, Stream stream) {
        mTabToStreamMap.put(key, stream);
    }

    int getTabToStreamSizeForTesting() {
        return mTabToStreamMap.size();
    }

    private void onDisplayStyleChanged(UiConfig.DisplayStyle newDisplayStyle) {
        if (mSectionHeaderModel != null) {
            mSectionHeaderModel.set(
                    SectionHeaderListProperties.IS_NARROW_WINDOW_ON_TABLET_KEY,
                    newDisplayStyle.horizontal < HorizontalDisplayStyle.WIDE);
        }
    }

    public @ClosedReason int getClosedReason() {
        return mClosedReason;
    }
}
