// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.TabResumptionDataProviderFactory;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallbacks;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * The Coordinator for the tab resumption module, which can be embedded by surfaces like NTP or
 * Start surface.
 */
public class TabResumptionModuleCoordinator implements ModuleProvider {
    protected final Context mContext;
    protected final ModuleDelegate mModuleDelegate;
    protected final TabResumptionDataProviderFactory mDataProviderFactory;
    protected final UrlImageProvider mUrlImageProvider;
    protected final PropertyModel mModel;

    protected TabResumptionDataProvider mDataProvider;
    protected TabResumptionModuleMediator mMediator;

    public TabResumptionModuleCoordinator(
            @NonNull Context context,
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull TabResumptionDataProviderFactory dataProviderFactory,
            @NonNull UrlImageProvider urlImageProvider,
            @NonNull ThumbnailProvider thumbnailProvider) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mDataProviderFactory = dataProviderFactory;
        mUrlImageProvider = urlImageProvider;
        mModel = new PropertyModel(TabResumptionModuleProperties.ALL_KEYS);
        SuggestionClickCallbacks wrappedClickCallbacks =
                new SuggestionClickCallbacks() {
                    @Override
                    public void onSuggestionClickByUrl(GURL gurl) {
                        mModuleDelegate.onUrlClicked(gurl, getModuleType());
                    }

                    @Override
                    public void onSuggestionClickByTabId(int tabId) {
                        moduleDelegate.onTabClicked(tabId, getModuleType());
                    }
                };
        mMediator =
                new TabResumptionModuleMediator(
                        /* context= */ mContext,
                        /* moduleDelegate= */ mModuleDelegate,
                        /* model= */ mModel,
                        /* urlImageProvider= */ mUrlImageProvider,
                        /* thumbnailProvider= */ thumbnailProvider,
                        /* statusChangedCallback= */ this::showModule,
                        /* seeMoreLinkClickCallback= */ this::onSeeMoreClicked,
                        /* suggestionClickCallbacks= */ wrappedClickCallbacks);
        mMediator.startSession(mDataProviderFactory.make());
    }

    public void destroy() {
        mMediator.endSession();
        mMediator.destroy();
        mUrlImageProvider.destroy();
    }

    /** Shows tab resumption module. */
    @Override
    public void showModule() {
        mMediator.loadModule();
    }

    /** Loads the Mediator with new Data Provider, and re-shows tab resumption module. */
    @Override
    public void updateModule() {
        mMediator.endSession();
        mMediator.startSession(mDataProviderFactory.make());
        mMediator.loadModule();
    }

    @Override
    public int getModuleType() {
        return mMediator.getModuleType();
    }

    @Override
    public void hideModule() {
        destroy();
    }

    @Override
    public String getModuleContextMenuHideText(Context context) {
        return mMediator.getModuleContextMenuHideText(context);
    }

    @Override
    public void onContextMenuCreated() {}

    private void onSeeMoreClicked() {
        mModuleDelegate.onUrlClicked(new GURL(UrlConstants.RECENT_TABS_URL), getModuleType());
    }
}
