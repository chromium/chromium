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

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleNotShownReason;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleShowConfig;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.visited_url_ranking.ScoredURLUserAction;
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
        private MultiTabObserver mLocalTabClosureObserver;

        private boolean mIsAlive;
        private Handler mHandler;

        // Configuration of the tab resumption module if shown, or null if hidden. This is used for
        // logging and for ShowHideHelper to check shown / hidden status.
        private @Nullable @ModuleShowConfig Integer mModuleShowConfig;

        private long mFirstLoadTime;
        private boolean mIsStable;
        private SuggestionBundle mBundle;
        private @ResultStrength int mStrength;
        private final CallbackController mCallbackController;

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
            mLocalTabClosureObserver =
                    new MultiTabObserver() {
                        @Override
                        public void onClosingStateChanged(Tab tab, boolean closing) {
                            if (!closing) return;

                            // Tab is being closed and only cleanup should be done. We'd like to
                            // reload, but doing so eagerly now is error-prone (in particular,
                            // mTabModel.getTabById() on the closing Tab would return non-null).
                            // Therefore defer reload by posting on current task runner.
                            assert ThreadUtils.runningOnUiThread();
                            PostTask.postTask(TaskTraits.UI_DEFAULT, mReloadSessionCallback);
                        }
                    };

            mCallbackController = new CallbackController();
            mModel.set(
                    TabResumptionModuleProperties.TAB_OBSERVER_CALLBACK,
                    (tab) -> mLocalTabClosureObserver.add(tab));
            mStrength = ResultStrength.TENTATIVE;
            mIsAlive = true;
        }

        void destroy() {
            if (mBundle != null) { // Do not check `mStrength`.
                recordSeenActionForEntries(mBundle.entries);
            }

            mCallbackController.destroy();
            mLocalTabClosureObserver.destroy();
            mDataProvider.setStatusChangedCallback(null);
            mDataProvider.destroy();
            if (mHandler != null) {
                mHandler.removeCallbacksAndMessages(null);
                mHandler = null;
            }

            // Activates if STABLE and FORCED_NULL results isn't seen, and timeout has not been
            // triggered yet. This includes the case where TENTATIVE tiles are quickly clicked by a
            // user.
            ensureStabilityAndLogMetrics(/* recordStabilityDelay= */ false, mModuleShowConfig);
            mIsAlive = false;
        }

        /** Records the "activated" action for the given `entry`. */
        void recordActivatedActionForEntry(SuggestionEntry entry) {
            if (entry.trainingInfo != null) {
                entry.trainingInfo.record(ScoredURLUserAction.ACTIVATED);
            }
        }

        /** Record the "seen" action for the given `entries`. */
        private void recordSeenActionForEntries(List<SuggestionEntry> entries) {
            // If needed, we can denoise this by requiring that rendered tiled have been scrolled
            // onto screen. Another idea is to require suggestions to exist for some time bound.
            for (SuggestionEntry entry : entries) {
                if (entry.trainingInfo != null) {
                    // If an entry has previously recorded() then this would be a no-op.
                    entry.trainingInfo.record(ScoredURLUserAction.SEEN);
                }
            }
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
                        TabResumptionModuleUtils.getCurrentTimeMs() - mFirstLoadTime);
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
            // expires then potentially-cached data are deemed good-enough, and rendered.
            //
            // A drawback of the placeholder-timeout approach is that having no updates is common.
            // So often placeholder would show, only to surrender to module rendered using
            // potentially-cached data that was readily available to start with;
            //
            // To better use potentially-cached data, we take the following hybrid approach:
            // * TENTATIVE / "fast path" data: Fetch potentially-cached data on initial load. If
            //   "something" then render and enter SHOWN. If "nothing" then stay in INIT and wait.
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
                mFirstLoadTime = TabResumptionModuleUtils.getCurrentTimeMs();
            }

            mDataProvider.fetchSuggestions(this::onSuggestionReceived);
        }

        /** Handles `suggestions` data passed from mDataProvider.fetchSuggestions(). */
        private void onSuggestionReceived(@NonNull SuggestionsResult result) {
            // destroy() might have been called while the fetchSuggestions() is in flight. Abort if
            // that happens, to prevent interfering with the UI of a succeeding Session.
            if (!mIsAlive) return;

            if (mBundle != null && mStrength != ResultStrength.TENTATIVE) {
                // Suggestions have already been rendered, and is being overwritten. Record that the
                // old suggestions were seen if not TENTATIVE, which might not have the chance to
                // stay on screen long enough before being replaced.
                recordSeenActionForEntries(mBundle.entries);
            }

            List<SuggestionEntry> suggestions = result.suggestions;
            mBundle =
                    (suggestions != null && suggestions.size() > 0)
                            ? makeSuggestionBundle(suggestions)
                            : null;

            @ModuleShowConfig Integer prevModuleShowConfig = mModuleShowConfig;
            // This directly changes `mShowHideHelper` results.
            mModuleShowConfig = TabResumptionModuleMetricsUtils.computeModuleShowConfig(mBundle);

            @Nullable Callback<Integer> callback = createOnModuleShowConfigFinalizedCallback();
            boolean shouldLogMetrics = callback == null;

            mStrength = result.strength;
            if (mStrength == ResultStrength.TENTATIVE) {
                setPropertiesAndTriggerRender(mBundle, callback);
                // On first call, start timeout to transition to STABLE and log.
                if (mHandler == null) {
                    mHandler = new Handler();
                    mHandler.postDelayed(
                            () -> {
                                // Activates if the only strength seen is TENTATIVE. In this case,
                                // TENTATIVE suggestions is considered stable.
                                if (shouldLogMetrics) {
                                    ensureStabilityAndLogMetrics(
                                            /* recordStabilityDelay= */ false, mModuleShowConfig);
                                }
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

            } else if (mStrength == ResultStrength.STABLE) {
                mShowHideHelper.maybeNotifyModuleDelegate();

                setPropertiesAndTriggerRender(mBundle, callback);
                if (shouldLogMetrics) {
                    ensureStabilityAndLogMetrics(
                            /* recordStabilityDelay= */ true, mModuleShowConfig);
                }
            } else if (mStrength == ResultStrength.FORCED_NULL) {
                assert !mShowHideHelper.shouldShow();
                mShowHideHelper.maybeNotifyModuleDelegate();
                // Activates if STABLE was never encountered. In this case, TENTATIVE suggestions
                // are considered stable (therefore log `prevModuleShowConfig`).
                setPropertiesAndTriggerRender(mBundle, callback);
                if (shouldLogMetrics) {
                    ensureStabilityAndLogMetrics(
                            /* recordStabilityDelay= */ false, prevModuleShowConfig);
                }
            }

            if (mBundle != null) {
                mLocalTabClosureObserver.clear();
                TabModel tabModel = mTabModelSelectorSupplier.get().getModel(false);
                for (SuggestionEntry entry : mBundle.entries) {
                    if (entry.isLocalTab()) {
                        Tab tab = tabModel.getTabById(entry.getLocalTabId());
                        assert tab != null; // isSuggestionValid() filtering ensures this.
                        mLocalTabClosureObserver.add(tab);
                    }
                }
            }
        }

        @Nullable
        private Callback<Integer> createOnModuleShowConfigFinalizedCallback() {
            if (mModuleShowConfig == null || isModuleShowConfigFinalized(mModuleShowConfig)) {
                return null;
            }

            return mCallbackController.makeCancelable(
                    moduleShowConfig -> {
                        mModuleShowConfig = moduleShowConfig;
                        ensureStabilityAndLogMetrics(
                                /* recordStabilityDelay= */ true, mModuleShowConfig);
                        mModel.set(
                                TabResumptionModuleProperties
                                        .ON_MODULE_SHOW_CONFIG_FINALIZED_CALLBACK,
                                null);
                    });
        }

        /** Returns whether the type of ModuleShowConfig is a finalized one. */
        private boolean isModuleShowConfigFinalized(@ModuleShowConfig int moduleShowConfig) {
            return moduleShowConfig != ModuleShowConfig.SINGLE_TILE_ANY
                    && moduleShowConfig != ModuleShowConfig.DOUBLE_TILE_ANY;
        }
    }

    // If TENTATIVE suggestions were received, and the following duration has elapsed without
    // receiving a STABLE suggestion, then consider the results stable and log accordingly.
    private static final long STABILITY_TIMEOUT_MS = TimeUnit.SECONDS.toMillis(7);

    private final Context mContext;
    private final ModuleDelegate mModuleDelegate;
    private final PropertyModel mModel;
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;

    protected final UrlImageProvider mUrlImageProvider;
    protected final Runnable mReloadSessionCallback;
    protected final Runnable mStatusChangedCallback;
    protected final SuggestionClickCallback mSuggestionClickCallback;
    private final ShowHideHelper mShowHideHelper;

    private Session mSession;

    public TabResumptionModuleMediator(
            @NonNull Context context,
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            @NonNull PropertyModel model,
            @NonNull UrlImageProvider urlImageProvider,
            @NonNull Runnable reloadSessionCallback,
            @NonNull Runnable statusChangedCallback,
            @NonNull Runnable seeMoreLinkClickCallback,
            @NonNull SuggestionClickCallback suggestionClickCallback) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mModel = model;
        mUrlImageProvider = urlImageProvider;
        mReloadSessionCallback = reloadSessionCallback;
        mStatusChangedCallback = statusChangedCallback;
        mSuggestionClickCallback =
                (SuggestionEntry entry) -> {
                    mSession.recordActivatedActionForEntry(entry);
                    suggestionClickCallback.onSuggestionClicked(entry);
                };
        mShowHideHelper = new ShowHideHelper();

        mModel.set(
                TabResumptionModuleProperties.TAB_MODEL_SELECTOR_SUPPLIER,
                mTabModelSelectorSupplier);
        mModel.set(TabResumptionModuleProperties.URL_IMAGE_PROVIDER, mUrlImageProvider);
        mModel.set(
                TabResumptionModuleProperties.SEE_MORE_LINK_CLICK_CALLBACK,
                seeMoreLinkClickCallback);
        mModel.set(TabResumptionModuleProperties.CLICK_CALLBACK, mSuggestionClickCallback);
    }

    /** Starts a new Session with the given `dataProvider`. There should be no existing Session. */
    void startSession(@NonNull TabResumptionDataProvider dataProvider) {
        assert mSession == null;
        mSession = new Session(dataProvider, mStatusChangedCallback);
    }

    /** Ends the Session if one exists. If no Session exists then no-op (i.e., be forgiving). */
    void endSession() {
        if (mSession != null) {
            mSession.destroy();
            mSession = null;
        }
    }

    void destroy() {
        assert mSession == null;
        mModel.set(TabResumptionModuleProperties.URL_IMAGE_PROVIDER, null);
    }

    /**
     * Fetches new suggestions, creates SuggestionBundle, then updates `mModel`. If no data is
     * available then hides the module. See onSuggestionReceived() for details.
     */
    void loadModule() {
        mModel.set(TabResumptionModuleProperties.TRACKING_TAB, mModuleDelegate.getTrackingTab());
        mSession.maybeFetchSuggestionsAndRenderResults();
    }

    /**
     * @return Whether the given suggestion is qualified to be shown in UI.
     */
    private boolean isSuggestionValid(SuggestionEntry entry) {
        if (entry.isLocalTab()) {
            // We already detect closure the of a suggested Local Tab, and perform refresh. Below
            // guards against glitches where a Local Tab somehow gets closed, or is in the closing
            // state, but still get suggested.
            assert mTabModelSelectorSupplier.hasValue();
            TabModel tabModel = mTabModelSelectorSupplier.get().getModel(false);
            assert tabModel != null;
            Tab tab = tabModel.getTabById(entry.getLocalTabId());
            return tab != null && !tab.isClosing();
        }

        return !TextUtils.isEmpty(entry.title);
    }

    /**
     * Filters `suggestions` to choose up to MAX_TILES_NUMBER top ones, the returns the data in a
     * new SuggestionBundle instance.
     *
     * @param suggestions Retrieved suggestions with basic filtering, from most recent to least.
     */
    private SuggestionBundle makeSuggestionBundle(List<SuggestionEntry> suggestions) {
        long currentTimeMs = TabResumptionModuleUtils.getCurrentTimeMs();
        SuggestionBundle bundle = new SuggestionBundle(currentTimeMs);
        int maxTilesNumber = TabResumptionModuleUtils.TAB_RESUMPTION_MAX_TILES_NUMBER.getValue();
        boolean hasLocalTab = false;
        for (SuggestionEntry entry : suggestions) {
            // At most one local Tab can be shown on the Tab resumption module.
            if (hasLocalTab && entry.isLocalTab()) {
                continue;
            }

            if (isSuggestionValid(entry)) {
                boolean isHistoryEntry = entry.type == SuggestionEntryType.HISTORY;
                // The entry of type |HISTORY| will be in a single tile only.
                if (isHistoryEntry) {
                    if (!bundle.entries.isEmpty()) continue;
                    bundle.entries.add(entry);
                    break;
                }
                bundle.entries.add(entry);
                hasLocalTab |= entry.isLocalTab();
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
    private void setPropertiesAndTriggerRender(
            @Nullable SuggestionBundle bundle, @Nullable Callback<Integer> callback) {
        String title = null;
        boolean isVisible = false;
        if (bundle != null) {
            Resources res = mContext.getResources();
            title =
                    res.getQuantityString(
                            R.plurals.home_modules_tab_resumption_title, bundle.entries.size());
            isVisible = true;
        }
        mModel.set(
                TabResumptionModuleProperties.ON_MODULE_SHOW_CONFIG_FINALIZED_CALLBACK, callback);
        mModel.set(TabResumptionModuleProperties.SUGGESTION_BUNDLE, bundle);
        mModel.set(TabResumptionModuleProperties.TITLE, title);
        mModel.set(TabResumptionModuleProperties.IS_VISIBLE, isVisible);
    }
}
