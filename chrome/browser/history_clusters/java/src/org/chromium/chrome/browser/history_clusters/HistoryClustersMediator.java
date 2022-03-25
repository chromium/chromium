// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.NonNull;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

class HistoryClustersMediator {
    private final HistoryClustersProvider mHistoryClustersProvider;
    private final Context mContext;
    private final Resources mResources;
    private final ModelList mModelList;
    private final RoundedIconGenerator mIconGenerator;
    private final LargeIconBridge mLargeIconBridge;
    private Promise<HistoryClustersResult> mPromise;

    /**
     * Create a new HistoryClustersMediator.
     * @param historyClustersProvider Provider of history clusters data.
     * @param context Android context from which UI configuration should be derived.
     * @param resources Android resources object from which strings, colors etc. should be fetched.
     * @param modelList Model list to which fetched cluster data should be pushed to.
     * @param profile Profile from which we should access history/favicon data.
     */
    HistoryClustersMediator(@NonNull HistoryClustersProvider historyClustersProvider,
            @NonNull Context context, @NonNull Resources resources, @NonNull ModelList modelList,
            @NonNull Profile profile) {
        mHistoryClustersProvider = historyClustersProvider;
        mModelList = modelList;
        mContext = context;
        mResources = resources;
        mIconGenerator = FaviconUtils.createCircularIconGenerator(mContext);
        mLargeIconBridge = new LargeIconBridge(profile);
    }

    public void destroy() {
        mLargeIconBridge.destroy();
    }
}
