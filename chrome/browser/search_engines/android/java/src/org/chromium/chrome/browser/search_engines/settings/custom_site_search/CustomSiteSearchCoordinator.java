// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_site_search;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

@NullMarked
public class CustomSiteSearchCoordinator {
    private final ModelList mModelList;
    private final SimpleRecyclerViewAdapter mAdapter;

    public CustomSiteSearchCoordinator(Context context) {
        mModelList = new ModelList();
        mAdapter = new CustomSiteSearchAdapter(context, mModelList);
    }

    public void destroy() {
        mAdapter.destroy();
    }
}
