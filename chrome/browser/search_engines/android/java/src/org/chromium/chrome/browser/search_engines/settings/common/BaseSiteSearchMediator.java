// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineIconUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/**
 * A base mediator for Site Search settings pages. Handles standard actions such as observing {@link
 * TemplateUrlService}, cleaning up resources, and generating list items with favicons.
 */
@NullMarked
public abstract class BaseSiteSearchMediator
        implements TemplateUrlService.TemplateUrlServiceObserver {
    protected final Context mContext;
    protected final ModelList mModelList;
    protected final TemplateUrlService mTemplateUrlService;
    protected final LargeIconBridge mLargeIconBridge;
    protected final int mFaviconSize;
    protected final Map<GURL, Bitmap> mIconCache = new HashMap<>();

    /**
     * Constructs the base mediator and initializes core dependencies.
     *
     * @param context The current context.
     * @param modelList The modelList representing the UI list to be populated.
     * @param profile The current user profile.
     */
    public BaseSiteSearchMediator(Context context, ModelList modelList, Profile profile) {
        mContext = context;
        mModelList = modelList;
        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        mLargeIconBridge = new LargeIconBridge(profile);
        mFaviconSize = context.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
    }

    /**
     * Initializes the mediator by adding itself as an observer and populating the list. This should
     * be called during the mediator's initialization. We do not call this in the constructor
     * because subclasses may need to initialize their own fields first.
     */
    public void initializeTemplateUrlService() {
        mTemplateUrlService.addObserver(this);
        mTemplateUrlService.runWhenLoaded(this::refreshList);
    }

    /** Cleans up native resources and observers. Must be called when the UI is destroyed. */
    public void destroy() {
        mTemplateUrlService.removeObserver(this);
        mLargeIconBridge.destroy();
    }

    @Override
    public void onTemplateURLServiceChanged() {
        refreshList();
    }

    /**
     * Creates a standard ListItem for a given search engine. Binds the site name, shortcut,
     * generated menu, and a default/fetched favicon. Note that the caller is responsible to handle
     * the menu delegate.
     *
     * @param url The TemplateUrl representing the search engine.
     * @return A ListItem ready to be added to the ModelList.
     */
    protected ListItem createListItem(TemplateUrl url) {
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

    /**
     * Fetches the favicon for the search engine asynchronously and updates the PropertyModel.
     *
     * @param templateUrl The TemplateUrl representing the search engine.
     * @param model The PropertyModel to update once the favicon is fetched.
     */
    protected void fetchFavicon(TemplateUrl templateUrl, PropertyModel model) {
        // Since we're fetching the favicon from Google server, we're using the origin as the page
        // URL rather than using {@link TemplateUrl#getFaviconURL()}
        GURL pageUrl = new GURL(templateUrl.getURL()).getOrigin();
        executeIconUpdate(
                mContext,
                model,
                SiteSearchProperties.ICON,
                templateUrl,
                pageUrl,
                mLargeIconBridge,
                mIconCache);
    }

    /** Wrapper for the static SearchEngineIconUtils.updateIcon to allow mocking in tests. */
    @VisibleForTesting
    void executeIconUpdate(
            Context context,
            PropertyModel model,
            PropertyModel.WritableObjectPropertyKey<Bitmap> propertyKey,
            TemplateUrl templateUrl,
            GURL pageUrl,
            LargeIconBridge largeIconBridge,
            Map<GURL, Bitmap> iconCache) {
        SearchEngineIconUtils.updateIcon(
                context, model, propertyKey, templateUrl, pageUrl, largeIconBridge, iconCache);
    }

    /**
     * Called when the list needs to be repopulated (e.g., on initialization or data change).
     * Subclasses must implement this to clear the {@link ModelList} and add the relevant items.
     */
    protected abstract void refreshList();

    /**
     * Creates the popup menu delegate for a specific search engine item.
     *
     * @param url The TemplateUrl the menu actions will apply to.
     * @return A ListMenuDelegate to handle menu interactions, or null if no menu is needed.
     */
    protected abstract @Nullable ListMenuDelegate createMenuDelegate(TemplateUrl url);
}
