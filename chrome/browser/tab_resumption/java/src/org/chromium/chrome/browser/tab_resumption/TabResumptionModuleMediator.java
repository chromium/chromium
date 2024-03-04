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
    private static final int MAX_TILES_NUMBER = 2;

    private final Context mContext;
    private final ModuleDelegate mModuleDelegate;
    private final PropertyModel mModel;
    protected final TabResumptionDataProvider mDataProvider;
    protected final UrlImageProvider mUrlImageProvider;
    protected final SuggestionClickCallback mSuggestionClickCallback;

    private boolean mHasPreviousLoad;

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
        // Abort if the module has already loaded but is hidden.
        if (mHasPreviousLoad && !mModel.get(TabResumptionModuleProperties.IS_VISIBLE)) {
            return;
        }

        mDataProvider.fetchSuggestions(
                (List<SuggestionEntry> suggestions) -> {
                    SuggestionBundle bundle = null;
                    if (suggestions != null) {
                        if (suggestions.size() == 0) {
                            // TODO(crbug.com/1515325): Record metrics here.
                        } else {
                            bundle = makeSuggestionBundle(suggestions);
                        }
                    }
                    // On first load, send load status to Magic Stack to help it decide whether to
                    // show or to hide the module.
                    if (!mHasPreviousLoad) {
                        if (bundle != null) {
                            mModuleDelegate.onDataReady(getModuleType(), mModel);
                        } else {
                            mModuleDelegate.onDataFetchFailed(getModuleType());
                        }
                        mHasPreviousLoad = true;
                    }

                    mModel.set(
                            TabResumptionModuleProperties.SUGGESTION_BUNDLE,
                            bundle); // Triggers render.
                    mModel.set(TabResumptionModuleProperties.IS_VISIBLE, bundle != null);
                    if (bundle != null) {
                        // TODO(crbug.com/1515325): Record metrics here.
                        Resources res = mContext.getResources();
                        String title =
                                res.getQuantityString(
                                        R.plurals.home_modules_tab_resumption_title,
                                        bundle.entries.size());
                        mModel.set(TabResumptionModuleProperties.TITLE, title);
                    } else {
                        mModel.set(TabResumptionModuleProperties.TITLE, null);
                    }
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
        return context.getResources()
                .getString(R.string.tab_resumption_module_other_devices_context_menu_hide);
    }
}
