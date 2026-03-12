// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_site_search;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.settings.common.ExpandableSiteSearchMediator;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.search_engines.StarterPackId;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlCategory;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

@NullMarked
public class CustomSiteSearchMediator extends ExpandableSiteSearchMediator {
    private final Runnable mOnAddSearchEngine;
    private final Callback<TemplateUrl> mOnEditSearchEngine;

    public CustomSiteSearchMediator(
            Context context,
            ModelList modelList,
            Profile profile,
            Runnable onAddSearchEngine,
            Callback<TemplateUrl> onEditSearchEngine) {
        super(context, modelList, profile);
        mOnAddSearchEngine = onAddSearchEngine;
        mOnEditSearchEngine = onEditSearchEngine;

        initializeTemplateUrlService();
    }

    @Override
    protected void refreshList() {
        mModelList.clear();
        mHiddenItems.clear();

        List<TemplateUrl> urls =
                mTemplateUrlService.getTemplateUrlsByCategory(
                        TemplateUrlCategory.ACTIVE_SITE_SEARCH);

        setUpSiteSearchList(urls);
        setUpAddButton();
        setUpMoreButtonIfNeeded(urls.size());
    }

    private void setUpSiteSearchList(List<TemplateUrl> urls) {
        for (int i = 0; i < urls.size(); i++) {
            ListItem item = createListItem(urls.get(i));
            if (i < DEFAULT_MAX_ROWS) {
                mModelList.add(item);
            } else {
                mHiddenItems.add(item);
            }
        }
    }

    private void setUpAddButton() {
        PropertyModel addButtonModel =
                new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                        .with(SiteSearchProperties.ON_CLICK, v -> mOnAddSearchEngine.run())
                        .build();
        mModelList.add(new ListItem(SiteSearchProperties.ViewType.ADD, addButtonModel));
    }

    @Override
    protected ListMenuDelegate createMenuDelegate(TemplateUrl url) {
        return () -> {
            ModelList menuItems = new ModelList();
            // If url is from a starter pack, we can only deactivate it; otherwise, we can edit,
            // make default, deactivate and delete it.
            // TODO: Handle this in the native layer.
            if (url.getStarterPackId() != StarterPackId.NONE) {
                menuItems.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.site_search_list_menu_deactivate)
                                .build());
            } else {
                menuItems.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.site_search_list_menu_edit)
                                .build());
                menuItems.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.site_search_list_menu_make_default)
                                .build());
                menuItems.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.site_search_list_menu_deactivate)
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
        if (textId == R.string.site_search_list_menu_edit) {
            mOnEditSearchEngine.onResult(url);
        } else if (textId == R.string.site_search_list_menu_make_default) {
            mTemplateUrlService.setSearchEngine(url.getKeyword());
        } else if (textId == R.string.site_search_list_menu_deactivate) {
            mTemplateUrlService.deactivateSearchEngine(url.getKeyword());
        } else if (textId == R.string.site_search_list_menu_delete) {
            mTemplateUrlService.removeSearchEngine(url.getKeyword());
        }
    }

    boolean isExpandedForTesting() {
        return mIsExpanded;
    }
}
