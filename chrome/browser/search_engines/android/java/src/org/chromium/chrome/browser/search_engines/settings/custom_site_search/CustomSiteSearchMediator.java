// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_site_search;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineIconUtils;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlCategory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

@NullMarked
public class CustomSiteSearchMediator implements TemplateUrlService.TemplateUrlServiceObserver {
    private static final int DEFAULT_MAX_ROWS = 5;

    private final Context mContext;
    private final ModelList mModelList;
    private final TemplateUrlService mTemplateUrlService;
    private final LargeIconBridge mLargeIconBridge;
    private final int mFaviconSize;
    private final Map<GURL, Bitmap> mIconCache = new HashMap<GURL, Bitmap>();
    private final List<ListItem> mHiddenItems = new ArrayList<>();
    private boolean mIsExpanded;
    private final Runnable mOnAddSearchEngine;

    boolean isExpandedForTesting() {
        return mIsExpanded;
    }

    public CustomSiteSearchMediator(
            Context context, ModelList modelList, Profile profile, Runnable onAddSearchEngine) {
        mContext = context;
        mModelList = modelList;
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        mLargeIconBridge = new LargeIconBridge(profile);
        mFaviconSize = context.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mOnAddSearchEngine = onAddSearchEngine;

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
            TemplateUrl url = urls.get(i);
            PropertyModel model =
                    new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                            .with(SiteSearchProperties.SITE_NAME, url.getShortName())
                            .with(SiteSearchProperties.SITE_SHORTCUT, url.getKeyword())
                            .with(
                                    SiteSearchProperties.ON_CLICK,
                                    v -> {
                                        // TODO: Handle overflow menu button
                                    })
                            .with(
                                    SiteSearchProperties.ICON,
                                    FaviconUtils.createGenericFaviconBitmap(
                                            mContext, mFaviconSize, null))
                            .build();

            fetchFavicon(url, model);
            ListItem item = new ListItem(SiteSearchProperties.ViewType.SEARCH_ENGINE, model);
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

    private void onMoreButtonClicked(PropertyModel moreButtonModel) {
        mIsExpanded = !mIsExpanded;
        moreButtonModel.set(SiteSearchProperties.IS_EXPANDED, mIsExpanded);
        if (mIsExpanded) {
            mModelList.addAll(mHiddenItems);
        } else {
            // Remove items starting from index DEFAULT_MAX_ROWS + 2.
            // Index layout:
            // [0 to DEFAULT_MAX_ROWS - 1]                             : Site search list
            // [DEFAULT_MAX_ROWS]                                      : 'Add' button
            // [DEFAULT_MAX_ROWS + 1]                                  : 'More' button
            // [DEFAULT_MAX_ROWS + 2 to ... + 1 + mHiddenItems.size()] : Hidden items
            mModelList.removeRange(DEFAULT_MAX_ROWS + 2, mHiddenItems.size());
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
}
