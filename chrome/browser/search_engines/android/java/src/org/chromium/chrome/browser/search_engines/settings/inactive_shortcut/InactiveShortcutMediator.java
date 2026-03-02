// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.inactive_shortcut;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

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
import org.chromium.components.search_engines.StarterPackId;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlCategory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

@NullMarked
public class InactiveShortcutMediator implements TemplateUrlService.TemplateUrlServiceObserver {
    private static final int DEFAULT_MAX_ROWS = 5;

    private final Context mContext;
    private final ModelList mModelList;
    private final TemplateUrlService mTemplateUrlService;
    private final LargeIconBridge mLargeIconBridge;
    private final int mFaviconSize;
    private final Map<GURL, Bitmap> mIconCache = new HashMap<GURL, Bitmap>();
    private final List<TemplateUrl> mHiddenUrls = new ArrayList<>();
    private final List<ListItem> mHiddenItems = new ArrayList<>();
    private boolean mIsExpanded;

    boolean isExpandedForTesting() {
        return mIsExpanded;
    }

    public InactiveShortcutMediator(Context context, ModelList modelList, Profile profile) {
        mContext = context;
        mModelList = modelList;
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        mLargeIconBridge = new LargeIconBridge(profile);
        mFaviconSize = context.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);

        mTemplateUrlService.addObserver(this);
        mTemplateUrlService.runWhenLoaded(this::refreshList);
    }

    public void destroy() {
        mTemplateUrlService.removeObserver(this);
        mLargeIconBridge.destroy();
    }

    @Override
    public void onTemplateURLServiceChanged() {
        mIsExpanded = false;
        refreshList();
    }

    private void refreshList() {
        mModelList.clear();
        mHiddenUrls.clear();
        mHiddenItems.clear();

        List<TemplateUrl> urls =
                mTemplateUrlService.getTemplateUrlsByCategory(
                        TemplateUrlCategory.INACTIVE_SITE_SEARCH);

        setUpSiteSearchList(urls);
        setUpMoreButtonIfNeeded(urls.size());
    }

    private void setUpSiteSearchList(List<TemplateUrl> urls) {
        for (int i = 0; i < urls.size(); i++) {
            TemplateUrl url = urls.get(i);
            if (i < DEFAULT_MAX_ROWS) {
                mModelList.add(createListItem(url));
            } else {
                mHiddenUrls.add(url);
            }
        }
    }

    private ListItem createListItem(TemplateUrl url) {
        PropertyModel model =
                new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                        .with(SiteSearchProperties.SITE_NAME, url.getShortName())
                        .with(SiteSearchProperties.SITE_SHORTCUT, url.getKeyword())
                        .with(SiteSearchProperties.MENU_DELEGATE, createMenuDelegate(url))
                        .with(
                                SiteSearchProperties.ICON,
                                FaviconUtils.createGenericFaviconBitmap(
                                        mContext, mFaviconSize, null))
                        .build();

        fetchFavicon(url, model);
        return new ListItem(SiteSearchProperties.ViewType.SEARCH_ENGINE, model);
    }

    private void setUpMoreButtonIfNeeded(int numUrls) {
        if (numUrls <= DEFAULT_MAX_ROWS) {
            return;
        }
        PropertyModel moreButtonModel =
                new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                        .with(SiteSearchProperties.IS_EXPANDED, mIsExpanded)
                        .build();
        moreButtonModel.set(
                SiteSearchProperties.ON_CLICK, v -> onMoreButtonClicked(moreButtonModel));
        mModelList.add(new ListItem(SiteSearchProperties.ViewType.MORE, moreButtonModel));
    }

    // TODO: We're using a single RecyclerView + ModelList to display the list + More button +
    // the hidden list items. Consider splitting into one RecyclerView for the list + More button
    // and another RecyclerView for the hidden list items.
    private void onMoreButtonClicked(PropertyModel moreButtonModel) {
        mIsExpanded = !mIsExpanded;
        moreButtonModel.set(SiteSearchProperties.IS_EXPANDED, mIsExpanded);
        if (mIsExpanded) {
            // Lazy load models and fetch favicons on the first expansion.
            if (mHiddenItems.isEmpty() && !mHiddenUrls.isEmpty()) {
                for (TemplateUrl url : mHiddenUrls) {
                    mHiddenItems.add(createListItem(url));
                }
            }
            mModelList.addAll(mHiddenItems);
        } else {
            // Remove items starting from index DEFAULT_MAX_ROWS + 1.
            // Index layout:
            // [0 to DEFAULT_MAX_ROWS - 1]                             : Site search list
            // [DEFAULT_MAX_ROWS]                                      : 'More' button
            // [DEFAULT_MAX_ROWS + 1 to ... + 1 + mHiddenItems.size()] : Hidden items
            mModelList.removeRange(DEFAULT_MAX_ROWS + 1, mHiddenItems.size());
        }
    }

    private void fetchFavicon(TemplateUrl url, PropertyModel model) {
        GURL faviconUrl = url.getFaviconURL();
        if (faviconUrl == null) {
            return;
        }
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
        return () -> {
            ModelList menuItems = new ModelList();
            // If url is from a starter pack, we can only activate it; otherwise, we can make
            // activate, make default and delete it.
            // TODO: Handle this in the native layer.
            menuItems.add(
                    new ListItemBuilder()
                            .withTitleRes(R.string.site_search_list_menu_activate)
                            .build());
            if (url.getStarterPackId() == StarterPackId.NONE) {
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
        if (textId == R.string.site_search_list_menu_activate) {
            mTemplateUrlService.activateSearchEngine(url.getKeyword());
        } else if (textId == R.string.site_search_list_menu_make_default) {
            mTemplateUrlService.setSearchEngine(url.getKeyword());
        } else if (textId == R.string.site_search_list_menu_delete) {
            mTemplateUrlService.removeSearchEngine(url.getKeyword());
        }
    }
}
