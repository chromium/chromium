// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

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
    protected final TabResumptionDataProvider mDataProvider;
    protected final UrlImageProvider mUrlImageProvider;
    protected final PropertyModel mModel;
    protected final TabResumptionModuleView mModuleView;
    protected PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    protected final TabResumptionModuleMediator mMediator;

    public TabResumptionModuleCoordinator(
            TabResumptionDataProvider dataProvider,
            UrlImageProvider urlImageProvider,
            SuggestionClickCallback suggestionClickCallback,
            ViewStub viewStub) {
        mDataProvider = dataProvider;
        mUrlImageProvider = urlImageProvider;

        SuggestionClickCallback wrappedCallback =
                (GURL gurl) -> {
                    suggestionClickCallback.onSuggestionClick(gurl);
                    // TODO(crbug.com/1515325): Record metrics here.
                };
        mModel =
                new PropertyModel.Builder(TabResumptionModuleProperties.ALL_KEYS)
                        .with(TabResumptionModuleProperties.DATA_PROVIDER, mDataProvider)
                        .with(TabResumptionModuleProperties.URL_IMAGE_PROVIDER, mUrlImageProvider)
                        .with(TabResumptionModuleProperties.CLICK_CALLBACK, wrappedCallback)
                        .build();
        mModuleView = (TabResumptionModuleView) viewStub.inflate();
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mModuleView, new TabResumptionModuleViewBinder());
        mMediator = createMediator();
        mDataProvider.setStatusChangedCallback(this::reload);
    }

    protected TabResumptionModuleMediator createMediator() {
        return new TabResumptionModuleMediator(mModel);
    }

    public void destroy() {
        mDataProvider.setStatusChangedCallback(null);
        mMediator.destroy();
        mPropertyModelChangeProcessor.destroy();
        mModuleView.destroy();

        mUrlImageProvider.destroy();
        mDataProvider.destroy();
    }

    /** Reloads the most up-to-date data and rerenders the module. */
    public void reload() {
        mMediator.reload();
    }

    public PropertyModel getModelForTesting() {
        return mModel;
    }
}
