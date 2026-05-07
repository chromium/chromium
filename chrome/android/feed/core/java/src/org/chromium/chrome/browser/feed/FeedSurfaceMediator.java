// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.ObserverList;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.FeedSurfaceProvider.RestoringState;
import org.chromium.chrome.browser.feed.Stream.ContentChangedListener;
import org.chromium.chrome.browser.gesturenav.GestureNavigationUtils;
import org.chromium.chrome.browser.new_tab_url.DseNewTabUrlManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.chrome.browser.ui.signin.signin_promo.NtpSigninPromoDelegate;
import org.chromium.chrome.browser.ui.signin.signin_promo.SigninPromoCoordinator;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;
import org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger.ClosedReason;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.List;
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
                IdentityManager.Observer {

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
                                    this::onPromoStateChange,
                                    SetupListModuleUtils::isSetupListActive));
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

        /**
         * @return Whether the {@link FeedSignInPromo} should be created.
         */
        public static boolean shouldCreatePromo() {
            NtpSigninPromoDelegate.resetNtpSyncPromoLimitsIfHiddenForTooLong();
            return !ChromeSharedPreferences.getInstance()
                            .readBoolean(
                                    ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false)
                    && !getSuppressionStatus();
        }

        private static boolean getSuppressionStatus() {
            long suppressedFrom =
                    SigninPreferencesManager.getInstance()
                            .getNewTabPageSigninPromoSuppressionPeriodStart();
            if (suppressedFrom == 0) return false;
            long currentTime = System.currentTimeMillis();
            long suppressedTo = suppressedFrom + NtpSigninPromoDelegate.getSuppressionPeriodMs();
            if (suppressedFrom <= currentTime && currentTime < suppressedTo) {
                return true;
            }
            SigninPreferencesManager.getInstance()
                    .clearNewTabPageSigninPromoSuppressionPeriodStart();
            return false;
        }
    }

    /** Internal implementation of Stream.StreamsMediator. */
    @VisibleForTesting
    public class StreamsMediatorImpl implements Stream.StreamsMediator {
        @Override
        public void refreshStream() {
            mCoordinator.nonSwipeRefresh();
        }
    }

    public static void setPrefForTest(
            PrefChangeRegistrar prefChangeRegistrar, PrefService prefService) {
        sTestPrefChangeRegistar = prefChangeRegistrar;
        FeedFeatures.setFakePrefsForTest(prefService);
    }

    private static @Nullable PrefChangeRegistrar sTestPrefChangeRegistar;
    private static final int SPAN_COUNT_SMALL_WIDTH = 1;
    private static final int SPAN_COUNT_LARGE_WIDTH = 2;
    private static final int SMALL_WIDTH_DP = 700;

    private final FeedSurfaceCoordinator mCoordinator;
    private final Context mContext;
    private final Profile mProfile;
    private final PrefChangeRegistrar mPrefChangeRegistrar;
    private final SigninManager mSigninManager;
    private final TemplateUrlService mTemplateUrlService;
    private final FeedActionDelegate mActionDelegate;
    private @Nullable OnLayoutChangeListener mOnLayoutChangeListener;
    private @Nullable SnapScrollHelper mSnapScrollHelper;

    private final SettableNonNullObservableSupplier<Integer> mGetRestoringStateSupplier =
            ObservableSuppliers.createNonNull(RestoringState.WAITING_TO_RESTORE);

    private @Nullable OnScrollListener mStreamScrollListener;
    private @Nullable RecyclerViewAnimationFinishDetector mStreamScrollAnimationFinishDetector;
    private final ObserverList<ScrollListener> mScrollListeners = new ObserverList<>();
    private @Nullable ContentChangedListener mStreamContentChangedListener;
    private @Nullable MemoryPressureCallback mMemoryPressureCallback;
    private @Nullable FeedSigninPromo mSigninPromo;
    private @Nullable RecyclerViewAnimationFinishDetector mRecyclerViewAnimationFinishDetector;

    private boolean mFeedEnabled;
    private boolean mTouchEnabled = true;
    private boolean mStreamContentChanged;
    private int mThumbnailWidth;
    private int mThumbnailHeight;
    private int mThumbnailScrollY;
    private int mHeaderCount;

    /** Whether the Feed content is loading. */
    private boolean mIsLoadingFeed;

    private @Nullable FeedScrollState mRestoreScrollState;
    private int mPositionToRestore = RecyclerView.NO_POSITION;

    // Track single main stream.
    private @Nullable Stream mStreamHolder;
    private @Nullable Stream mCurrentStream;
    // Whether we're currently adding the streams. If this is true, streams should not be bound yet.
    // This avoids automatically binding the first stream when it's added.
    private boolean mSettingUpStreams;
    private final boolean mIsNewTabSearchEngineUrlAndroidEnabled;
    private boolean mIsPropertiesInitializedForStream;
    private @ClosedReason int mClosedReason = ClosedReason.SUSPEND_APP;

    /**
     * @param coordinator The {@link FeedSurfaceCoordinator} that interacts with this class.
     * @param context The current context.
     * @param snapScrollHelper The {@link SnapScrollHelper} that handles snap scrolling.
     * @param profile The {@link Profile} for the current user.
     */
    FeedSurfaceMediator(
            FeedSurfaceCoordinator coordinator,
            Context context,
            @Nullable SnapScrollHelper snapScrollHelper,
            FeedActionDelegate actionDelegate,
            Profile profile) {
        mCoordinator = coordinator;
        mContext = context;
        mSnapScrollHelper = snapScrollHelper;
        mProfile = profile;
        var signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        assertNonNull(signinManager);
        mSigninManager = signinManager;
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(mProfile);
        mActionDelegate = actionDelegate;
        mIsNewTabSearchEngineUrlAndroidEnabled =
                DseNewTabUrlManager.isNewTabSearchEngineUrlAndroidEnabled();

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

        mRecyclerViewAnimationFinishDetector = new RecyclerViewAnimationFinishDetector();
        // This works around the bug that the out-of-screen toolbar is not brought back together
        // with the new tab page view when it slides down. This is because the RecyclerView
        // animation may not finish when content changed event is triggered and thus the new tab
        // page layout view may still be partially off screen.
        mStreamContentChangedListener =
                contents ->
                        assumeNonNull(mRecyclerViewAnimationFinishDetector)
                                .runWhenAnimationComplete(this::onContentsChanged);

        initialize();
    }

    private void updateLayout(boolean isSmallLayoutWidth) {
        ListLayoutHelper listLayoutHelper =
                mCoordinator.getHybridListRenderer().getListLayoutHelper();
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                || listLayoutHelper == null
                || mCurrentStream == null) {
            return;
        }
        int spanCount = isSmallLayoutWidth ? SPAN_COUNT_SMALL_WIDTH : SPAN_COUNT_LARGE_WIDTH;
        boolean res = listLayoutHelper.setColumnCount(spanCount);
        assert res : "Failed to set column count on Feed";
    }

    /** Clears any dependencies. */
    void destroy() {
        // 1. Remove all observers first to stop all incoming events.
        mTemplateUrlService.removeObserver(this);

        // 2. Clean up Mediator-owned UI listeners.
        if (mOnLayoutChangeListener != null) {
            mCoordinator.getView().removeOnLayoutChangeListener(mOnLayoutChangeListener);
            mOnLayoutChangeListener = null;
        }

        if (mSnapScrollHelper != null) {
            mSnapScrollHelper.destroy();
            mSnapScrollHelper = null;
        }

        // 3. Clear the internal observer list.
        mScrollListeners.clear();

        // 4. Destroy properties and streams.
        destroyPropertiesForStream();
        // Only removes mPrefChangeRegistrar's observers when Feeds is destroyed. They can't be
        // removed in destroyPropertiesForStream() which is called in updateContent() and may cause
        // Feeds doesn't load when it is turned on after DSE is changed.
        mPrefChangeRegistrar.removeObserver(Pref.ENABLE_SNIPPETS);
        mPrefChangeRegistrar.removeObserver(Pref.ENABLE_SNIPPETS_BY_DSE);
        mPrefChangeRegistrar.destroy();

        // 5. Null out large references.
        mStreamHolder = null;
        mCurrentStream = null;
        mStreamContentChangedListener = null;
        if (mRecyclerViewAnimationFinishDetector != null) {
            mRecyclerViewAnimationFinishDetector.destroy();
            mRecyclerViewAnimationFinishDetector = null;
        }
        mRestoreScrollState = null;
    }

    private void initialize() {
        if (mSnapScrollHelper == null) return;

        // Listen for layout changes on the NewTabPageView itself to catch changes in scroll
        // position that are due to layout changes after e.g. device rotation. This contrasts with
        // regular scrolling, which is observed through an OnScrollListener.
        mOnLayoutChangeListener =
                (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    if (mSnapScrollHelper == null) return;

                    mCoordinator.getView().postOnAnimation(mSnapScrollHelper::handleScroll);
                    float pixelToDp = mContext.getResources().getDisplayMetrics().density;
                    int widthDp = (int) ((right - left) / pixelToDp);
                    updateLayout(widthDp < SMALL_WIDTH_DP);
                };
        mCoordinator.getView().addOnLayoutChangeListener(mOnLayoutChangeListener);
    }

    /**
     * Update the feed content based on whether it is enabled or disabled per enterprise policy.
     * When the feed is disabled, the feed content is completely gone.
     */
    void updateContent() {
        // See https://crbug.com/40075985.
        if (ApplicationStatus.isEveryActivityDestroyed()) return;

        mFeedEnabled = FeedFeatures.isFeedEnabled(mProfile);
        if (mFeedEnabled && mStreamHolder != null) {
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
        mCoordinator.updateHeaderText(headerText);
        mStreamHolder = mainStream;
        bindStream(mStreamHolder);

        mSettingUpStreams = false;

        mStreamScrollAnimationFinishDetector = new RecyclerViewAnimationFinishDetector();
        mStreamScrollListener =
                new OnScrollListener() {
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
                                                        assumeNonNull(
                                                                        mStreamScrollAnimationFinishDetector)
                                                                .runWhenAnimationComplete(null);
                                                    };
                                            assumeNonNull(mStreamScrollAnimationFinishDetector)
                                                    .runWhenAnimationComplete(onComplete);
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
        if (!mCoordinator.isActive() || !isSuggestionsVisible()) {
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
        return mStreamHolder != null;
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
        if (mStreamHolder != null) {
            bindStream(mStreamHolder);
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
        } else {
            mCoordinator.updateHeaderViews(/* signinPromoView= */ null);
        }
    }

    /**
     * Determines whether a signin promo should be shown.
     *
     * @return Whether the FeedSigninPromo should be visible.
     */
    private boolean shouldShowSigninPromo() {
        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            return false;
        }
        boolean shouldCreatePromo = FeedSigninPromo.shouldCreatePromo();
        if (!shouldCreatePromo) {
            return false;
        }
        if (mSigninPromo == null) {
            mSigninPromo = new FeedSigninPromo(isSuggestionsVisible());
        }
        return mSigninPromo.canShowPromo();
    }

    /** Clear any dependencies related to the {@link Stream}. */
    @VisibleForTesting
    void destroyPropertiesForStream() {
        if (mStreamScrollListener != null) {
            mCoordinator.getRecyclerView().removeOnScrollListener(mStreamScrollListener);
            mStreamScrollListener = null;
        }

        if (mStreamScrollAnimationFinishDetector != null) {
            mStreamScrollAnimationFinishDetector.destroy();
            mStreamScrollAnimationFinishDetector = null;
        }

        if (mMemoryPressureCallback != null) {
            MemoryPressureListener.removeCallback(mMemoryPressureCallback);
            mMemoryPressureCallback = null;
        }

        if (mSigninPromo != null) {
            mSigninPromo.destroy();
            mSigninPromo = null;
        }

        if (mStreamHolder != null) {
            assumeNonNull(mStreamContentChangedListener);
            mStreamHolder.removeOnContentChangedListener(mStreamContentChangedListener);
            mStreamHolder.destroy();
            mStreamHolder = null;
        }

        unbindStream();

        mPrefChangeRegistrar.removeObserver(Pref.ARTICLES_LIST_VISIBLE);
        mSigninManager.getIdentityManager().removeObserver(this);

        mIsPropertiesInitializedForStream = false;

        if (mCoordinator.getSurfaceScope() != null) {
            mCoordinator.getSurfaceScope().getLaunchReliabilityLogger().cancelPendingEvents();
        }
    }

    /**
     * Show or hide the feed. When the feed is hidden, the discover off message will be shown.
     *
     * <p>Called when a settings change or update to this/another NTP caused the feed to show/hide.
     */
    void showOrHideFeed() {
        // It is possible that showOrHideFeed() is called when the surface which contains the
        // Feeds isn't visible or headers of streams haven't been added, returns here.
        // See https://crbug.com/40072900 and https://crbug.com/40073830.
        if (!mIsPropertiesInitializedForStream) {
            return;
        }

        boolean suggestionsVisible = isSuggestionsVisible();

        if (mSigninPromo != null) {
            mSigninPromo.setCanShowPersonalizedSuggestions(suggestionsVisible);
        }
        if (suggestionsVisible) {
            assumeNonNull(mCoordinator.getSurfaceLifecycleManager()).show();
        }
        mStreamContentChanged = true;

        String headerText = getHeaderText(suggestionsVisible);
        mCoordinator.updateHeaderText(headerText);

        // If feed turned on, we bind the last stream that was visible. Else unbind it.
        if (suggestionsVisible) {
            rebindStream();
        } else {
            unbindStream();
        }
    }

    /** Returns the feed header text. */
    private @Nullable String getHeaderText(boolean isExpanded) {
        if (isExpanded) {
            // Returns null to indicate that no feed header is shown.
            return null;
        }
        Resources res = mContext.getResources();
        if (mTemplateUrlService.isDefaultSearchEngineGoogle()) {
            return res.getString(R.string.ntp_discover_off);
        }
        return res.getString(R.string.ntp_discover_off_branded);
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

    private PrefService getPrefService() {
        return FeedFeatures.getPrefService(mProfile);
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
    public NonNullObservableSupplier<Integer> getRestoringStateSupplier() {
        return mGetRestoringStateSupplier;
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

        void destroy() {
            if (mFinishedCallback != null) {
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

    public boolean isSuggestionsVisible() {
        return getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE);
    }

    public boolean isFeedEnabled() {
        return mFeedEnabled;
    }

    public @ClosedReason int getClosedReason() {
        return mClosedReason;
    }
}
