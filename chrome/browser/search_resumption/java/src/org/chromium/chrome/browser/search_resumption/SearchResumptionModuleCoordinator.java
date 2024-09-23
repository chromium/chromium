// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import android.view.ViewGroup;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_resumption.SearchResumptionTileBuilder.OnSuggestionClickCallback;
import org.chromium.chrome.browser.search_resumption.SearchResumptionUserData.SuggestionResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * The Coordinator for search resumption module which can be embedded by surfaces like NTP or Start
 * surface.
 */
public class SearchResumptionModuleCoordinator {
    private final SearchResumptionModuleMediator mMediator;
    private final SearchResumptionTileBuilder mTileBuilder;

    public SearchResumptionModuleCoordinator(
            ViewGroup parent,
            Tab tabToTrack,
            Tab currentTab,
            Profile profile,
            int moduleContainerStbuId,
            SuggestionResult cachedSuggestions) {
        OnSuggestionClickCallback callback =
                (gurl) -> {
                    currentTab.loadUrl(new LoadUrlParams(gurl));
                    RecordUserAction.record(SearchResumptionModuleUtils.ACTION_CLICK);
                };
        mTileBuilder = new SearchResumptionTileBuilder(callback);
        mMediator =
                new SearchResumptionModuleMediator(
                        parent.findViewById(moduleContainerStbuId),
                        tabToTrack,
                        currentTab,
                        profile,
                        mTileBuilder,
                        cachedSuggestions);
    }

    public void destroy() {
        mMediator.destroy();
        mTileBuilder.destroy();
    }
}
