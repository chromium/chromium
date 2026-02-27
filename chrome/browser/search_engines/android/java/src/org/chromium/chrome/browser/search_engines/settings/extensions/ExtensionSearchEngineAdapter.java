// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.extensions;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties.ViewType;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchViewBinder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

@NullMarked
class ExtensionSearchEngineAdapter extends SimpleRecyclerViewAdapter {
    ExtensionSearchEngineAdapter(Context context, ModelList modelList) {
        super(modelList);
        registerType(
                ViewType.SEARCH_ENGINE,
                parent -> {
                    View view =
                            LayoutInflater.from(context)
                                    .inflate(R.layout.site_search_engine_item, parent, false);
                    view.setTag(new SiteSearchViewBinder.ViewHolder(view));
                    return view;
                },
                SiteSearchViewBinder::bind);
    }
}
