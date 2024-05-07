// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleNotShownReason;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleShowConfig;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallbacks;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.concurrent.TimeUnit;

/** The Mediator for the tab resumption module. */
public class TabResumptionModuleMediator {

    /** Helper to get module show / hide status, and notify ModuleDelegate when needed. */
    private class ShowHideHelper {
        // W.r.t. Magic Stack the module has 3 states: {INIT, SHOWN, GONE}.
        //   * INIT: The initial state where the module is not shown, but has the potential to.
        //   * SHOWN: The module is shown.
        //   * GONE: The terminal state where the module is hidden *and remains so*.
        // Transitions and how to trigger them via `mModuleDelegate`:
        //   * INIT -> SHOWN: Call onDataReady() to show the module.
        //   * INIT -> GONE: Call onDataFetchFailed() to hide the module. This is also triggered
        //     by Magic Stack timeout if the module stays in INIT too long.
        //   * SHOWN -> GONE: Call removeModule() to hide the already-shown module.
        //
        // The following variables are used to represent the state. Encoding:
        //   INIT: mHasNotifiedShow = false, mHasNotifiedHide = false.
        //   SHOWN: mHasNotifiedShow = true, mHasNotifiedHide = false.
        //   GONE: mHasNotifiedShow = (any), mHasNotifiedHide = true.
        private boolean mHasNotifiedShow;
        private boolean mHasNotifiedHide;

        /** Returns the current show / hide status (true = should show). */
        boolean shouldShow() {
            return mSession.mModuleShowConfig != null;
        }

        /**
         * Updates the state machine and if needed, makes "best effort" ModuleDelegate calls so the
         * module's shows / hide state reflects the logical show / hide status, while satisfying
         * Magic Stack constraints (e.g., when a module hides it can no longer show).
         */
        void maybeNotifyModuleDelegate() {
            if (mHasNotifiedHide) return; // Reached terminal state GONE.

            if (shouldShow()) {
                if (!mHasNotifiedShow) { // INIT -> SHOWN.
                    mModuleDelegate.onDataReady(getModuleType(), mModel);
                    mHasNotifiedShow = true;
                }
            } else {
                if (mHasNotifiedShow) { // SHOWN -> GONE.
                    mModuleDelegate.removeModule(getModuleType());
                } else { // INIT -> GONE.
                    mModuleDelegate.onDataFetchFailed(getModuleType());
                }
                mHasNotifiedHide = true;
            }
        }
    }

    /**
     * The Session inner class encapsulates ephemeral states and dynamics that are subject to reset
     * on module reload.
     */
    private class Session {
        private final TabResumptionDataProvider mDataProvider;

        private boolean mIsAlive;
        private Handler mHandler;

        // Configuration of the tab resumption module if shown, or null if hidden. This is used for
        // logging and for ShowHideHelper to check shown / hidden status.
        private @Nullable @ModuleShowConfig Integer mModuleShowConfig;

        private long mFirstLoadTime;
        private boolean mIsStable;

        /**
         * @param dataProvider TabResumptionDataProvider instance owned by this class.
         * @param statusChangedCallback Callback for `mDataProvider` to pass status change events
         *     while the Session is alive.
         */
        Session(
                @NonNull TabResumptionDataProvider dataProvider,
                @NonNull Runnable statusChangedCallback) {
            mDataProvider = dataProvider;
            mDataProvider.setStatusChangedCallback(statusChangedCallback);
            mIsAlive = true;
        }

        void destroy() {
            mDataProvider.setStatusChangedCallback(null);
            mDataProvider.destroy();
            if (mHandler != null) {
                mHandler.removeCallbacksAndMessages(null);
                mHandler = null;
            }
            // Activates if STABLE and FORCED_NULL results isn't seen, and timeout has triggered
            // yet.
            // This includes the case where TENTATIVE tiles are quickly clicked by a user.
            ensureStabilityAndLogMetrics(/* recordStabilityDelay= */ false, mModuleShowConfig);
            mIsAlive = false;
        }

        /**
         * If not yet done so, declares that results are stable, and logs module state.
         *
         * @param recordStabilityDelay Whether to record how long it takes to get and render stable
         *     results from "slow path".
         * @param moduleShowConfig The ModuleShowConfig to log. If null, then logs NO_SUGGESTIONS
         *     NotShownReason instead.
         */
        private void ensureStabilityAndLogMetrics(
                boolean recordStabilityDelay,
                @Nullable @ModuleShowConfig Integer moduleShowConfig) {
            // Activates only on first call.
            if (mIsStable) {
                return;
            }

            mIsStable = true;
            if (recordStabilityDelay
                    && (moduleShowConfig == null
                            || moduleShowConfig.intValue() != ModuleShowConfig.SINGLE_TILE_LOCAL)) {
                // Log only if Foreign Session suggestions exist.
                TabResumptionModuleMetricsUtils.recordStabilityDelay(
                        getCurrentTimeMs() - mFirstLoadTime);
            }
            if (moduleShowConfig == null) {
                TabResumptionModuleMetricsUtils.recordModuleNotShownReason(
                        ModuleNotShownReason.NO_SUGGESTIONS);
            } else {
                TabResumptionModuleMetricsUtils.recordModuleShowConfig(moduleShowConfig.intValue());
            }
        }

        /**
         * Fetches new suggestions, creates SuggestionBundle, then updates `mModel`. If no data is
         * available then hides the module. See onSuggestionReceived() for details.
         */
        void maybeFetchSuggestionsAndRenderResults() {
            // Load happens at initialization, and can be triggered later by various sources,
            // including `mDataProvider`. Suggestions data is retrieved via fetchSuggestions(), and
            // processed by a callback which does the following:

            // * Compute SuggestionBundle, which can be "something" or "nothing".
            // * Hide the module if "nothing"; render and show the module if "something".
            // * Invoke ModuleDelegate callbacks to communicate with Magic Stack. This is largely
            //   handled by `mShowHideHelper`. Refer to {INIT, SHOWN, GONE} states.
            // * Log the "stable" module state. The challenge is to determine when this happens.
            //
            // `mDataProvider` can take noticeable time to get desired data (e.g., from Sync). Idly
            // waiting for data is undesirable since:
            // * Staying in the INIT state prevents the Magic Stack from being shown -- even when
            //   other modules are ready.
            // * Under the hood, refreshing Sync-related data (e.g., Foreign Session) involves
            //   making a request followed by listening for update ping. However, the ping does not
            //   fire if there's no new data.
            //
            // One way to address the ping issue is to always trigger INIT -> SHOW, render a
            // placeholder, and use a timeout that races against data request. If the timeout
            // expires then cached data are deemed good-enough, and rendered.
            //
            // A drawback of the placeholder-timeout approach is that having no updates is common.
            // So often placeholder would show, only to surrender to module rendered using cached
            // data that was readily available to start with;
            //
            // To better use cached data, we take the following hybrid approach:
            // * TENTATIVE / "fast path" data: Fetch cached data on initial load. If "something"
            //   then render and enter SHOWN. If "nothing" then stay in INIT and wait.
            // * STABLE / "slow path" data: *If* "slow path" data arrives (can be triggered by
            //   `mDataProvider`), render UI with it, and enter SHOWN or GONE.
            //
            // Noting that TENTATIVE data can be {"nothing", "something"} and STABLE data can be
            // {"nothing", "something", "(never sent)"}, we have 6 cases:
            // * TENTATIVE "nothing" -> STABLE "nothing": On STABLE, do INIT -> GONE.
            // * TENTATIVE "nothing" -> STABLE "something": On STABLE, do INIT -> SHOWN.
            // * TENTATIVE "nothing" -> STABLE "(never sent)": On Magic Stack timeout do
            //   INIT -> GONE.
            // * TENTATIVE "something" -> STABLE "nothing": On TENTATIVE do INIT -> SHOWN;
            //   on STABLE do SHOWN -> GONE.
            // * TENTATIVE "something" -> STABLE "something": On TENTATIVE do INIT -> SHOWN;
            //   on STABLE re-render.
            // * TENTATIVE "something" -> STABLE "(never sent)": On TENTATIVE do INIT -> SHOWN.
            //
            // Logging the "stable" module state should be done on receiving the first STABLE data.
            // For the STABLE "(never sent)" case, this can be done in destroy().
            //
            // TODO (crbug.com/1515325): Handle manual refresh. Likely just use STABLE.
            //
            // Permission changes (e.g., disabling sync) may require module change beyond STABLE.
            // This is handled by FORCED_NULL data.

            // Skip work if the module instance is irreversibly hidden.
            if (mIsStable && !mShowHideHelper.shouldShow()) {
                return;
            }

            if (mFirstLoadTime == 0) {
                mFirstLoadTime = getCurrentTimeMs();
            }

            mDataProvider.fetchSuggestions(this::onSuggestionReceived);
        }

        /** Handles `suggestions` data passed from mDataProvider.fetchSuggestions(). */
        private void onSuggestionReceived(@NonNull SuggestionsResult result) {
            // destroy() might have been called while the fetchSuggestions() is in flight. Abort if
            // that happens, to prevent interfering with the UI of a succeeding Session.
            if (!mIsAlive) return;

            List<SuggestionEntry> suggestions = result.suggestions;
            SuggestionBundle bundle =
                    (suggestions != null && suggestions.size() > 0)
                            ? makeSuggestionBundle(suggestions)
                            : null;
            @ModuleShowConfig Integer prevModuleShowConfig = mModuleShowConfig;
            // This directly changes `mShowHideHelper` results.
            mModuleShowConfig = TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle);

            @ResultStrength int strength = result.strength;
            if (strength == ResultStrength.TENTATIVE) {
                setPropertiesAndTriggerRender(bundle);
                // On first call, start timeout to transition to STABLE and log.
                if (mHandler == null) {
                    mHandler = new Handler();
                    mHandler.postDelayed(
                            () -> {
                                // Activates if the only strength seen is TENTATIVE. In this case,
                                // TENTATIVE suggestions is considered stable.
                                ensureStabilityAndLogMetrics(
                                        /* recordStabilityDelay= */ false, mModuleShowConfig);
                                // Multiple TENTATIVE suggestions might have repeated attempts to
                                //  show / hide the module. Finalize if needed.
                                mShowHideHelper.maybeNotifyModuleDelegate();
                            },
                            STABILITY_TIMEOUT_MS);
                }
                // Hiding is not permanent while TENTATIVE, so only notify to show.
                if (mShowHideHelper.shouldShow()) {
                    mShowHideHelper.maybeNotifyModuleDelegate();
                }

            } else if (strength == ResultStrength.STABLE) {
                mShowHideHelper.maybeNotifyModuleDelegate();
                setPropertiesAndTriggerRender(bundle);
                ensureStabilityAndLogMetrics(/* recordStabilityDelay= */ true, mModuleShowConfig);

            } else if (strength == ResultStrength.FORCED_NULL) {
                assert !mShowHideHelper.shouldShow();
                mShowHideHelper.maybeNotifyModuleDelegate();
                // Activates if STABLE was never encountered. In this case, TENTATIVE suggestions
                // are considered stable (therefore log `prevModuleShowConfig`).
                setPropertiesAndTriggerRender(bundle);
                ensureStabilityAndLogMetrics(
                        /* recordStabilityDelay= */ false, prevModuleShowConfig);
            }
        }
    }

    // If TENTATIVE suggestions were received, and the following duration has elapsed without
    // receiving a STABLE suggestion, then consider the results stable and log accordingly.
    private static final long STABILITY_TIMEOUT_MS = TimeUnit.SECONDS.toMillis(7);

    private final Context mContext;
    private final ModuleDelegate mModuleDelegate;
    private final PropertyModel mModel;

    protected final UrlImageProvider mUrlImageProvider;
    protected final ThumbnailProvider mThumbnailProvider;
    protected final Runnable mStatusChangedCallback;
    protected final SuggestionClickCallbacks mSuggestionClickCallbacks;
    private final ShowHideHelper mShowHideHelper;

    private Session mSession;

    public TabResumptionModuleMediator(
            @NonNull Context context,
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull PropertyModel model,
            @NonNull UrlImageProvider urlImageProvider,
            @NonNull ThumbnailProvider thumbnailProvider,
            @NonNull Runnable statusChangedCallback,
            @NonNull Runnable seeMoreLinkClickCallback,
            @NonNull SuggestionClickCallbacks suggestionClickCallbacks) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mModel = model;
        mUrlImageProvider = urlImageProvider;
        mThumbnailProvider = thumbnailProvider;
        mStatusChangedCallback = statusChangedCallback;
        mSuggestionClickCallbacks = suggestionClickCallbacks;
        mShowHideHelper = new ShowHideHelper();

        mModel.set(TabResumptionModuleProperties.URL_IMAGE_PROVIDER, mUrlImageProvider);
        mModel.set(TabResumptionModuleProperties.THUMBNAIL_PROVIDER, mThumbnailProvider);
        mModel.set(
                TabResumptionModuleProperties.SEE_MORE_LINK_CLICK_CALLBACK,
                seeMoreLinkClickCallback);
        mModel.set(TabResumptionModuleProperties.CLICK_CALLBACK, mSuggestionClickCallbacks);
        mModel.set(
                TabResumptionModuleProperties.USE_SALIENT_IMAGE,
                TabResumptionModuleUtils.TAB_RESUMPTION_USE_SALIENT_IMAGE.getValue());
    }

    void startSession(@NonNull TabResumptionDataProvider dataProvider) {
        assert mSession == null;
        mSession = new Session(dataProvider, mStatusChangedCallback);
    }

    void endSession() {
        assert mSession != null;
        mSession.destroy();
        mSession = null;
    }

    void destroy() {
        assert mSession == null;
    }

    /** Returns the current time in ms since the epoch. */
    long getCurrentTimeMs() {
        return System.currentTimeMillis();
    }

    /**
     * Fetches new suggestions, creates SuggestionBundle, then updates `mModel`. If no data is
     * available then hides the module. See onSuggestionReceived() for details.
     */
    void loadModule() {
        mSession.maybeFetchSuggestionsAndRenderResults();
    }

    /**
     * @return Whether the given suggestion is qualified to be shown in UI.
     */
    private static boolean isSuggestionValid(SuggestionEntry entry) {
        return (entry instanceof LocalTabSuggestionEntry) || !TextUtils.isEmpty(entry.title);
    }

    /**
     * Filters `suggestions` to choose up to MAX_TILES_NUMBER top ones, the returns the data in a
     * new SuggestionBundle instance.
     *
     * @param suggestions Retrieved suggestions with basic filtering, from most recent to least.
     */
    private SuggestionBundle makeSuggestionBundle(List<SuggestionEntry> suggestions) {
        long currentTimeMs = getCurrentTimeMs();
        SuggestionBundle bundle = new SuggestionBundle(currentTimeMs);
        int maxTilesNumber = TabResumptionModuleUtils.TAB_RESUMPTION_MAX_TILES_NUMBER.getValue();
        for (SuggestionEntry entry : suggestions) {
            if (isSuggestionValid(entry)) {
                bundle.entries.add(entry);
                if (bundle.entries.size() >= maxTilesNumber) {
                    break;
                }
            }
        }
        return bundle;
    }

    int getModuleType() {
        return ModuleType.TAB_RESUMPTION;
    }

    String getModuleContextMenuHideText(Context context) {
        SuggestionBundle bundle = mModel.get(TabResumptionModuleProperties.SUGGESTION_BUNDLE);
        return context.getResources()
                .getQuantityString(
                        R.plurals.home_modules_context_menu_hide_tab, bundle.entries.size());
    }

    /** Computes and sets UI properties and triggers render. */
    private void setPropertiesAndTriggerRender(@Nullable SuggestionBundle bundle) {
        String title = null;
        boolean isVisible = false;
        if (bundle != null) {
            Resources res = mContext.getResources();
            title =
                    res.getQuantityString(
                            R.plurals.home_modules_tab_resumption_title, bundle.entries.size());
            isVisible = true;
        }
        mModel.set(TabResumptionModuleProperties.SUGGESTION_BUNDLE, bundle);
        mModel.set(TabResumptionModuleProperties.TITLE, title);
        mModel.set(TabResumptionModuleProperties.IS_VISIBLE, isVisible);
    }
}
