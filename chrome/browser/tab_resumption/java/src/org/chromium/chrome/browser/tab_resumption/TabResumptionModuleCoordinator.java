// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.view.ViewStub;

import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

/**
 * The Coordinator for the tab resumption module, which can be embedded by surfaces like NTP or
 * Start surface.
 */
public class TabResumptionModuleCoordinator {
    protected final Context mContext;
    protected final TabResumptionDataProvider mDataProvider;
    protected final UrlImageProvider mUrlImageProvider;
    protected final PropertyModel mModel;
    protected final TabResumptionModuleView mModuleView;
    protected PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    protected final TabResumptionModuleMediator mMediator;

    public TabResumptionModuleCoordinator(
            Context context,
            TabResumptionDataProvider dataProvider,
            UrlImageProvider urlImageProvider,
            SuggestionClickCallback suggestionClickCallback,
            ViewStub viewStub) {
        mContext = context;
        mDataProvider = dataProvider;
        mUrlImageProvider = urlImageProvider;
        mModel = new PropertyModel(TabResumptionModuleProperties.ALL_KEYS);
        mModuleView = (TabResumptionModuleView) viewStub.inflate();
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mModuleView, new TabResumptionModuleViewBinder());
        SuggestionClickCallback wrappedClickCallback =
                (GURL url) -> {
                    suggestionClickCallback.onSuggestionClick(url);
                    // TODO(crbug.com/1515325): Record metrics here.
                };
        mMediator =
                new TabResumptionModuleMediator(
                        mContext, mModel, mDataProvider, mUrlImageProvider, wrappedClickCallback);
        mDataProvider.setStatusChangedCallback(this::reload);
    }

    public void destroy() {
        mDataProvider.setStatusChangedCallback(null);
        mMediator.destroy();
        mPropertyModelChangeProcessor.destroy();
        mModuleView.destroy();

        mUrlImageProvider.destroy();
        mDataProvider.destroy();
    }

    /** Show tab resumption module. */
    public void showModule() {
        mMediator.loadModule();
    }

    /** Reloads tab resumption module in response to UI or data updates. */
    public void reload() {
        mMediator.loadModule();
    }
}
