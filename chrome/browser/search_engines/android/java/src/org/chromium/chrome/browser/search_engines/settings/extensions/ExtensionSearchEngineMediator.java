// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.extensions;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.settings.common.BaseSiteSearchMediator;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlCategory;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

@NullMarked
public class ExtensionSearchEngineMediator extends BaseSiteSearchMediator {

    public ExtensionSearchEngineMediator(Context context, ModelList modelList, Profile profile) {
        super(context, modelList, profile);

        initializeTemplateUrlService();
    }

    @Override
    protected void refreshList() {
        mModelList.clear();

        List<TemplateUrl> urls =
                mTemplateUrlService.getTemplateUrlsByCategory(TemplateUrlCategory.EXTENSION);

        for (TemplateUrl url : urls) {
            mModelList.add(createListItem(url));
        }
    }

    @Override
    protected @Nullable ListMenuDelegate createMenuDelegate(TemplateUrl url) {
        // TODO: Add menu delegate
        return null;
    }
}
