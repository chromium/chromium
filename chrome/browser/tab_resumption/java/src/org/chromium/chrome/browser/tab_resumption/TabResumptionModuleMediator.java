// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** The Mediator for the tab resumption module. */
public class TabResumptionModuleMediator {

    @interface ModuleState {
        int INIT = 0;
        int SHOWN = 1;
        int GONE = 2;
    }

    private static final int MAX_TILES_NUMBER = 2;

    private final Context mContext;
    private final ModuleDelegate mModuleDelegate;
    private final PropertyModel mModel;
    protected final TabResumptionDataProvider mDataProvider;
    protected final UrlImageProvider mUrlImageProvider;
    protected final SuggestionClickCallback mSuggestionClickCallback;

    private @ModuleState int mModuleState;

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
        mModuleState = ModuleState.INIT;

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
     * available then hides the module.
     */
    void loadModule() {
        if (mModuleState == ModuleState.GONE) return;

        mDataProvider.fetchSuggestions(
                (List<SuggestionEntry> suggestions) -> {
                    int nextModuleState =
                            (suggestions != null && suggestions.size() > 0)
                                    ? ModuleState.SHOWN
                                    : ModuleState.GONE;

                    if (mModuleState == ModuleState.INIT) {
                        // On initial load, send load status to Magic Stack to help it decide
                        // whether to show or to hide the module.
                        if (nextModuleState == ModuleState.SHOWN) {
                            mModuleDelegate.onDataReady(getModuleType(), mModel);
                        } else {
                            mModuleDelegate.onDataFetchFailed(getModuleType());
                        }
                    } else if (mModuleState == ModuleState.SHOWN
                            && nextModuleState == ModuleState.GONE) {
                        // Transition from SHOWN to GONE is permanent.
                        mModuleDelegate.removeModule(getModuleType());
                    }

                    if (nextModuleState == ModuleState.SHOWN) {
                        // TODO(crbug.com/1515325): Record metrics here.
                        Resources res = mContext.getResources();
                        SuggestionBundle bundle = makeSuggestionBundle(suggestions);
                        String title =
                                res.getQuantityString(
                                        R.plurals.home_modules_tab_resumption_title,
                                        bundle.entries.size());
                        // Trigger render.
                        mModel.set(TabResumptionModuleProperties.SUGGESTION_BUNDLE, bundle);
                        mModel.set(TabResumptionModuleProperties.TITLE, title);
                        mModel.set(TabResumptionModuleProperties.IS_VISIBLE, true);
                    } else {
                        // Trigger render.
                        mModel.set(TabResumptionModuleProperties.SUGGESTION_BUNDLE, null);
                        mModel.set(TabResumptionModuleProperties.TITLE, null);
                        mModel.set(TabResumptionModuleProperties.IS_VISIBLE, false);
                    }

                    mModuleState = nextModuleState;
                });
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
}
