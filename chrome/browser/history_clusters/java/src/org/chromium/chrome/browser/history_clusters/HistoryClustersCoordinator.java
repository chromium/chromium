// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * Root component for the HistoryClusters UI component, which displays lists of related history
 * visits grouped into clusters.
 */
public class HistoryClustersCoordinator {
    private final HistoryClustersQueryManager mHistoryClustersQueryManager;
    private final HistoryClustersMediator mMediator;
    private final ModelList mModelList;
    private final Context mContext;

    /**
     * Construct a new HistoryClustersCoordinator.
     * @param profile The profile from which the coordinator should access history data.
     * @param context Android context from which UI configuration (strings, colors etc.) should be
     *         derived.
     */
    public HistoryClustersCoordinator(@NonNull Profile profile, @NonNull Context context) {
        mContext = context;
        mHistoryClustersQueryManager = new HistoryClustersQueryManager(profile);
        mModelList = new ModelList();
        mMediator = new HistoryClustersMediator(
                mHistoryClustersQueryManager, context, context.getResources(), mModelList, profile);
    }

    public void destroy() {
        mHistoryClustersQueryManager.destroy();
        mMediator.destroy();
    }
}