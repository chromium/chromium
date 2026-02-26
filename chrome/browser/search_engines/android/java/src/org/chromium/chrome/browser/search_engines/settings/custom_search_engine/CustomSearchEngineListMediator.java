// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineIconUtils;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

@NullMarked
public class CustomSearchEngineListMediator
        implements TemplateUrlService.TemplateUrlServiceObserver {
    private final Context mContext;
    private final ModelList mModelList;
    private final TemplateUrlService mTemplateUrlService;
    private final LargeIconBridge mLargeIconBridge;
    private final int mFaviconSize;
    private final Map<GURL, Bitmap> mIconCache = new HashMap<GURL, Bitmap>();
    private final Callback<TemplateUrl> mOnEditSearchEngine;

    public CustomSearchEngineListMediator(
            Context context,
            ModelList modelList,
            Profile profile,
            Callback<TemplateUrl> onEditSearchEngine) {
        mContext = context;
        mModelList = modelList;
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        mLargeIconBridge = new LargeIconBridge(profile);
        mFaviconSize = context.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mOnEditSearchEngine = onEditSearchEngine;

        mTemplateUrlService.addObserver(this);
        mTemplateUrlService.runWhenLoaded(this::refreshList);
    }

    public void destroy() {
        mTemplateUrlService.removeObserver(this);
        mLargeIconBridge.destroy();
    }

    @Override
    public void onTemplateURLServiceChanged() {
        refreshList();
    }

    private void refreshList() {
        // TODO: Currently we only have around 5 rows here, we use the brute force approach to clear
        // the list. But it would be great to check if there's any better way to handle this.
        mModelList.clear();

        // TODO: Get only default search engines in template url services after the API is
        // available.
        List<TemplateUrl> urls = mTemplateUrlService.getTemplateUrls();
        TemplateUrl defaultSearchEngine = mTemplateUrlService.getDefaultSearchEngineTemplateUrl();
        for (TemplateUrl url : urls) {
            PropertyModel model =
                    new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                            .with(SiteSearchProperties.SITE_NAME, url.getShortName())
                            .with(SiteSearchProperties.SITE_SHORTCUT, url.getKeyword())
                            .with(
                                    SiteSearchProperties.MENU_DELEGATE,
                                    url.equals(defaultSearchEngine)
                                            ? null
                                            : createMenuDelegate(url))
                            .with(
                                    SiteSearchProperties.ICON,
                                    FaviconUtils.createGenericFaviconBitmap(
                                            mContext, mFaviconSize, null))
                            .build();

            fetchFavicon(url, model);
            mModelList.add(new ListItem(SiteSearchProperties.ViewType.SEARCH_ENGINE, model));
        }
    }

    private void fetchFavicon(TemplateUrl url, PropertyModel model) {
        GURL faviconUrl = url.getFaviconURL();
        if (faviconUrl == null) return;

        SearchEngineIconUtils.updateIcon(
                mContext,
                model,
                SiteSearchProperties.ICON,
                url,
                faviconUrl,
                mLargeIconBridge,
                mIconCache);
    }

    private ListMenuDelegate createMenuDelegate(TemplateUrl url) {
        return new ListMenuDelegate() {
            @Override
            public ListMenu getListMenu() {
                ModelList menuItems = new ModelList();
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
                                .withTitleRes(R.string.site_search_list_menu_delete)
                                .build());

                return BrowserUiListMenuUtils.getBasicListMenu(
                        mContext,
                        menuItems,
                        (model, view) -> {
                            int textId = model.get(ListMenuItemProperties.TITLE_ID);
                            onMenuItemClicked(textId, url);
                        });
            }
        };
    }

    @VisibleForTesting
    void onMenuItemClicked(int textId, TemplateUrl url) {
        if (R.string.site_search_list_menu_edit == textId) {
            mOnEditSearchEngine.onResult(url);
        } else if (R.string.site_search_list_menu_make_default == textId) {
            mTemplateUrlService.setSearchEngine(url.getKeyword());
        } else if (R.string.site_search_list_menu_delete == textId) {
            mTemplateUrlService.removeSearchEngine(url.getKeyword());
        }
    }
}
