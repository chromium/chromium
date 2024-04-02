// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** The Mediator for the tab resumption module. */
public class TabResumptionModuleMediator {

    private static final int MAX_TILES_NUMBER = 2;

    private final Context mContext;
    private final ModuleDelegate mModuleDelegate;
    private final PropertyModel mModel;

    protected final TabResumptionDataProvider mDataProvider;
    protected final UrlImageProvider mUrlImageProvider;
    protected final SuggestionClickCallback mSuggestionClickCallback;

    // The number of tiles shown as a result of the latest suggestion. This determines whether the
    // module is visible (iff non-0), and is useful for logging.
    private int mTileCount;

    private boolean mIsStable;

    public TabResumptionModuleMediator(
            Context context,
            ModuleDelegate moduleDelegate,
            PropertyModel model,
            TabResumptionDataProvider dataProvider,
            UrlImageProvider urlImageProvider,
            SuggestionClickCallback suggestionClickCallback) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mModel = model;
        mDataProvider = dataProvider;
        mUrlImageProvider = urlImageProvider;
        mSuggestionClickCallback = suggestionClickCallback;
        mTileCount = 0;

        mModel.set(TabResumptionModuleProperties.URL_IMAGE_PROVIDER, mUrlImageProvider);
        mModel.set(TabResumptionModuleProperties.CLICK_CALLBACK, mSuggestionClickCallback);
    }

    void destroy() {
        // Active if STABLE and FORCED_NULL results were never seen.
        ensureStabilityAndLogMetrics(mTileCount);
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
        // Load happens at initialization, and can be triggered later by various sources, including
        // `mDataProvider`. Suggestions data is retrieved via fetchSuggestions(), and processed by a
        // callback which does the following:
        // * Compute SuggestionBundle, which can be "something" or "nothing".
        // * Hide the module if "nothing"; render and show the module if "something".
        // * Invoke ModuleDelegate callbacks to communicate with Magic Stack.
        //   * The module has 3 states: {INIT, SHOWN, GONE}.
        //     * INIT: The initial state where the module is not shown, but has the potential to.
        //     * SHOWN: The module is shown.
        //     * GONE: The terminal state where the module is hidden *and remains so*.
        //   * Transitions and how to trigger them:
        //     * INIT -> SHOWN: Call onDataReady() to show the module.
        //     * INIT -> GONE: Call onDataFetchFailed() to hide the module. This is also triggered
        //       by Magic Stack timeout if the module stays in INIT too long.
        //     * SHOWN -> GONE: Call removeModule() to hide the already-shown module.
        // * Log the "stable" module state. The challenge is to determine when
        //
        // `mDataProvider` can take noticeable time to get desired data (e.g., from Sync). Idly
        // waiting for data is undesirable since:
        // * Staying in the INIT state prevents the Magic Stack from being shown -- even when other
        //   modules are ready.
        // * Under the hood, refreshing Sync-related data (e.g., Foreign Session) involves making a
        //   request followed by listening for update ping. However, the ping does not fire if
        //   there's no new data.
        //
        // One way to address the ping issue is to always trigger INIT -> SHOW, render a
        // placeholder, and use a timeout that races against data request. If the timeout expires
        // then cached data are deemed good-enough, and rendered.
        //
        // A drawback of the placeholder-timeout approach is that having no updates is common. So
        // often placeholder would show, only to surrender to module rendered using cached data that
        // was readily available to start with;
        //
        // To better use cached data, we take the following hybrid approach:
        // * TENTATIVE / "fast path" data: Fetch cached data on initial load. If "something" then
        //   render and enter SHOWN. If "nothing" then stay in INIT and wait.
        // * STABLE / "slow path" data: *If* "slow path" data arrives (can be triggered by
        //   `mDataProvider`), render UI with it, and enter SHOWN or GONE.
        //
        // Noting that TENTATIVE data can be {"nothing", "something"} and STABLE data can be
        // {"nothing", "something", "(never sent)"}, we have 6 cases:
        // * TENTATIVE "nothing" -> STABLE "nothing": On STABLE, do INIT -> GONE.
        // * TENTATIVE "nothing" -> STABLE "something": On STABLE, do INIT -> SHOWN.
        // * TENTATIVE "nothing" -> STABLE "(never sent)": On Magic Stack timeout do INIT -> GONE.
        // * TENTATIVE "something" -> STABLE "nothing": On TENTATIVE do INIT -> SHOWN;
        //   on STABLE do SHOWN -> GONE.
        // * TENTATIVE "something" -> STABLE "something": On TENTATIVE do INIT -> SHOWN;
        //   on STABLE re-render.
        // * TENTATIVE "something" -> STABLE "(never sent)": On TENTATIVE do INIT -> SHOWN.
        //
        // Logging the "stable" module state should be done on receiving the first STABLE data. For
        // the STABLE "(never sent)" case, this can be done in destroy().
        //
        // TODO (crbug.com/1515325): Perhaps use timeout to capture logging for the
        // STABLE "(never sent)" case.
        //
        // TODO (crbug.com/1515325): Handle manual refresh. Likely just use STABLE.
        //
        // Permission changes (e.g., disabling sync) may require module change beyond STABLE. This
        // is handled by FORCED_NULL data.

        // Skip work if the module instance is irreversibly hidden.
        if (mIsStable && mTileCount == 0) {
            return;
        }

        mDataProvider.fetchSuggestions(this::onSuggestionReceived);
    }

    /**
     * @return Whether the given suggestion is qualified to be shown in UI.
     */
    private static boolean isSuggestionValid(SuggestionEntry entry) {
        return !TextUtils.isEmpty(entry.title);
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
        for (SuggestionEntry entry : suggestions) {
            if (isSuggestionValid(entry)) {
                bundle.entries.add(entry);
                if (bundle.entries.size() >= MAX_TILES_NUMBER) {
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

    /** If not yet done so, declare that results are stable, and log module state. */
    void ensureStabilityAndLogMetrics(int numTilesShown) {
        if (mIsStable) {
            return;
        }

        // TODO(crbug.com/1515325): Record metrics here.
        mIsStable = true;
    }

    /** Computes and sets UI properties and triggers render. */
    private void setPropertiesAndTriggerRender(@Nullable SuggestionBundle bundle) {
        @Nullable String title = null;
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

    /** Handles `suggestions` data passed from mDataProvider.fetchSuggestions(). */
    private void onSuggestionReceived(@NonNull SuggestionsResult result) {
        List<SuggestionEntry> suggestions = result.suggestions;
        @Nullable
        SuggestionBundle bundle =
                (suggestions != null && suggestions.size() > 0)
                        ? makeSuggestionBundle(suggestions)
                        : null;
        int nextTileCount = (bundle == null) ? 0 : bundle.entries.size();

        @ResultStrength int strength = result.strength;
        if (strength == ResultStrength.TENTATIVE) {
            setPropertiesAndTriggerRender(bundle);
            // Do not log yet.

        } else if (strength == ResultStrength.STABLE) {
            if (mTileCount == 0 && nextTileCount == 0) {
                mModuleDelegate.onDataFetchFailed(getModuleType());
            }
            setPropertiesAndTriggerRender(bundle);
            ensureStabilityAndLogMetrics(nextTileCount);

        } else if (strength == ResultStrength.FORCED_NULL) {
            assert nextTileCount == 0;
            setPropertiesAndTriggerRender(bundle);
            // Active if STABLE was never encountered, using previous number of tiles shown.
            ensureStabilityAndLogMetrics(mTileCount);
        }

        // If module visibility changes.
        if ((mTileCount == 0) != (nextTileCount == 0)) {
            if (nextTileCount != 0) { // Hidden to shown.
                mModuleDelegate.onDataReady(getModuleType(), mModel);
            } else { // Shown to hidden.
                mModuleDelegate.removeModule(getModuleType());
            }
        }

        mTileCount = nextTileCount;
    }
}
