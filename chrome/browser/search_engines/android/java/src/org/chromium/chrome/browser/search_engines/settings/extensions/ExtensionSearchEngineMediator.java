// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.extensions;

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

import java.util.HashMap;
import java.util.List;
import java.util.Map;

@NullMarked
public class ExtensionSearchEngineMediator
        implements TemplateUrlService.TemplateUrlServiceObserver {
    private final Context mContext;
    private final ModelList mModelList;
    private final TemplateUrlService mTemplateUrlService;
    private final LargeIconBridge mLargeIconBridge;
    private final int mFaviconSize;
    private final Map<GURL, Bitmap> mIconCache = new HashMap<GURL, Bitmap>();

    public ExtensionSearchEngineMediator(Context context, ModelList modelList, Profile profile) {
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
        refreshList();
    }

    private void refreshList() {
        mModelList.clear();

        List<TemplateUrl> urls =
                mTemplateUrlService.getTemplateUrlsByCategory(TemplateUrlCategory.EXTENSION);

        for (TemplateUrl url : urls) {
            PropertyModel model =
                    new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                            .with(SiteSearchProperties.SITE_NAME, url.getShortName())
                            .with(SiteSearchProperties.SITE_SHORTCUT, url.getKeyword())
                            // TODO: Add menu delegate
                            .with(SiteSearchProperties.MENU_DELEGATE, null)
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
