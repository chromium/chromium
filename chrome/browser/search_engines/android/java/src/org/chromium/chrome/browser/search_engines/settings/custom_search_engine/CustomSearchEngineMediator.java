// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.settings.common.BaseSiteSearchMediator;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlCategory;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

@NullMarked
public class CustomSearchEngineMediator extends BaseSiteSearchMediator {
    private final Callback<TemplateUrl> mOnEditSearchEngine;
    private final Callback<TemplateUrl> mOnRemoveSearchEngine;

    public CustomSearchEngineMediator(
            Context context,
            ModelList modelList,
            Profile profile,
            Callback<TemplateUrl> onEditSearchEngine,
            Callback<TemplateUrl> onRemoveSearchEngine) {
        super(context, modelList, profile);
        mOnEditSearchEngine = onEditSearchEngine;
        mOnRemoveSearchEngine = onRemoveSearchEngine;

        initializeTemplateUrlService();
    }

    @Override
    protected void refreshList() {
        mModelList.clear();

        List<TemplateUrl> urls =
                mTemplateUrlService.getTemplateUrlsByCategory(TemplateUrlCategory.DEFAULT);
        for (TemplateUrl url : urls) {
            mModelList.add(createListItem(url));
        }
    }

    @Override
    protected @Nullable ListMenuDelegate createMenuDelegate(TemplateUrl url) {
        return () -> {
            TemplateUrl defaultSearchEngine =
                    mTemplateUrlService.getDefaultSearchEngineTemplateUrl();
            ModelList menuItems = new ModelList();

            menuItems.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.site_search_list_menu_edit)
                            .build());

            // Only allow users to make default or remove if it is not DSE.
            if (!url.equals(defaultSearchEngine)) {
                menuItems.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.site_search_list_menu_make_default)
                                .build());
                menuItems.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.site_search_list_menu_delete)
                                .build());
            }

            return BrowserUiListMenuUtils.getBasicListMenu(
                    mContext,
                    menuItems,
                    (model, view) -> {
                        int textId = model.get(ListMenuItemProperties.TITLE_ID);
                        onMenuItemClicked(textId, url);
                    });
        };
    }

    @VisibleForTesting
    void onMenuItemClicked(int textId, TemplateUrl url) {
        if (R.string.site_search_list_menu_edit == textId) {
            mOnEditSearchEngine.onResult(url);
        } else if (R.string.site_search_list_menu_make_default == textId) {
            mTemplateUrlService.setSearchEngine(url.getKeyword());
        } else if (R.string.site_search_list_menu_delete == textId) {
            mOnRemoveSearchEngine.onResult(url);
        }
    }
}
