// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** The Mediator for the tab resumption module. */
public class TabResumptionModuleMediator {

    @interface ModuleStage {
        // Module is just initialized, awaiting suggestions.
        int INIT = 0;
        // Module got tentatively suggestions, awaiting final suggestions.
        int TENTATIVE = 1;
        // Module suggestions are stable, but can still get hidden if shown.
        int STABLE = 2;
    }

    private static final int MAX_TILES_NUMBER = 2;

    private final Context mContext;
    private final ModuleDelegate mModuleDelegate;
    private final PropertyModel mModel;
    protected final TabResumptionDataProvider mDataProvider;
    protected final UrlImageProvider mUrlImageProvider;
    protected final SuggestionClickCallback mSuggestionClickCallback;

    private @ModuleStage int mModuleStage;
    private boolean mDoShow;

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
        mModuleStage = ModuleStage.INIT;

        mModel.set(TabResumptionModuleProperties.URL_IMAGE_PROVIDER, mUrlImageProvider);
        mModel.set(TabResumptionModuleProperties.CLICK_CALLBACK, mSuggestionClickCallback);
    }

    void destroy() {}

    /** Returns the current time in ms since the epoch. */
    long getCurrentTimeMs() {
        return System.currentTimeMillis();
    }

    /**
     * Fetches new suggestions, creates SuggestionBundle, then updates `mModel`. If no data is
     * available then hides the module. See onSuggestionReceived() for details.
     */
    void loadModule() {
        // In each module instance, loadModule() can get called a few times:
        // * Initial call: `mDataProvider` returns quickly with stale or even empty (e.g., at
        //   startup before data is loaded) suggestions, but can also trigger a data load request
        //   that leads to Update calls.
        //   * Use: Provides tentative suggestions, allowing Magic Stack to show earlier.
        //   * Possible results: {nothing, something}.
        // * Update calls: Returning data load request or user action can cause `mDataProvider` to
        //   trigger module refresh, brings control flow back here (after noticeable delay). The
        //   resulting suggestions would be fresher than Initial call's.
        //   * Use: Provides up-to-date suggestions.
        //   * Possible results: {nothing, something, (never called)}.
        //
        // Meanwhile, Magic Stack expects the following ModuleDelegate callbacks:
        // * onDataReady(): To show module, and unblock Magic Stack wait.
        // * onDataFetchFailed(): To hide module, and unblock Magic Stack wait.
        // * removeModule(): To hide module after onDataReady() is already called.
        // * (Once removeModule() is called, there's no way to re-show module).
        // onDataReady() and onDataFetchFailed() are mutually exclusive; removeModule() can only
        // be called once, and only if onDataReady() is called.
        //
        // Expected cases (Initial call --> update call):
        // * Init nothing --> nothing: onDataFetchFailed() on update call.
        // * Init nothing --> something: onDataReady() on update call.
        // * Init nothing --> (never called): Magic stack times out and hides module.
        // * Init something --> nothing: onDataReady() on init call; removeModule() on update call.
        // * Init something --> something: onDataReady() on init call, rerender on update call.
        // * Init something --> (never called): onDataReady() on init call.
        // Special cases:
        // * Init any --> (after long delay) something: Should treat as Init any --> (never called).
        // * Init any --> something --> nothing: Corresponds to the case where data permission
        //   changes and the module should disappear: removeModule() on second update call.
        // * Init any --> something --> something: Update when module is stable: The second
        //   update call should be ignored.

        // If module is stable and hidden, stay that way and don't bother fetching.
        if (mModuleStage == ModuleStage.STABLE && !mDoShow) return;

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

    /** Computes and sets UI properties and triggers render. */
    private void setPropertiesAndTriggerRender(List<SuggestionEntry> suggestions) {
        @Nullable SuggestionBundle bundle = null;
        @Nullable String title = null;
        boolean isVisible = false;
        if (suggestions != null && suggestions.size() > 0) {
            Resources res = mContext.getResources();
            bundle = makeSuggestionBundle(suggestions);
            title =
                    res.getQuantityString(
                            R.plurals.home_modules_tab_resumption_title, bundle.entries.size());
            isVisible = true;
        }
        mModel.set(TabResumptionModuleProperties.SUGGESTION_BUNDLE, bundle);
        mModel.set(TabResumptionModuleProperties.TITLE, title);
        mModel.set(TabResumptionModuleProperties.IS_VISIBLE, isVisible);
    }

    /**
     * Handles `suggestions` data passed from mDataProvider.fetchSuggestions() by advancing
     * `mModuleStage`, updating `mDoShow`, and making appropriate `mModuleDelegate` calls.
     */
    private void onSuggestionReceived(List<SuggestionEntry> suggestions) {
        boolean nextDoShow = (suggestions != null && suggestions.size() > 0);

        if (mModuleStage == ModuleStage.INIT) {
            if (nextDoShow) {
                mModuleDelegate.onDataReady(getModuleType(), mModel);
            }
            setPropertiesAndTriggerRender(suggestions);
            mModuleStage = ModuleStage.TENTATIVE;

        } else if (mModuleStage == ModuleStage.TENTATIVE) {
            if (nextDoShow) {
                if (!mDoShow) {
                    mModuleDelegate.onDataReady(getModuleType(), mModel);
                } // Else skip; onDataReady() was already called.
            } else {
                if (mDoShow) {
                    mModuleDelegate.removeModule(getModuleType());
                } else {
                    mModuleDelegate.onDataFetchFailed(getModuleType());
                }
            }
            setPropertiesAndTriggerRender(suggestions);
            mDataProvider.setIsStable(true);
            mModuleStage = ModuleStage.STABLE;

        } else if (mModuleStage == ModuleStage.STABLE) {
            // `mDataProvider` is made stable in the TENTATIVE stage, so we'd expect the flow to
            // only reaches here if `suggestions` is empty. However, to account for possible race
            // condition it's better to be defensive and check again.
            if (!nextDoShow) {
                if (mDoShow) {
                    mModuleDelegate.removeModule(getModuleType());
                }
                setPropertiesAndTriggerRender(null);
            } // Else enforce stability, and don't trigger render.
        }

        mDoShow = nextDoShow;
    }
}
