// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.TabResumptionDataProviderFactory;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
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
    protected final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    protected final TabResumptionDataProviderFactory mDataProviderFactory;
    protected final UrlImageProvider mUrlImageProvider;
    protected final PropertyModel mModel;

    protected TabResumptionDataProvider mDataProvider;
    protected TabResumptionModuleMediator mMediator;

    public TabResumptionModuleCoordinator(
            @NonNull Context context,
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            @NonNull TabResumptionDataProviderFactory dataProviderFactory,
            @NonNull UrlImageProvider urlImageProvider) {
        mContext = context;
        mModuleDelegate = moduleDelegate;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mDataProviderFactory = dataProviderFactory;
        mUrlImageProvider = urlImageProvider;
        mModel = new PropertyModel(TabResumptionModuleProperties.ALL_KEYS);
        SuggestionClickCallback suggstionClickCallback =
                (SuggestionEntry entry) -> {
                    if (entry.isLocalTab()) {
                        mModuleDelegate.onTabClicked(entry.getLocalTabId(), getModuleType());
                    } else {
                        if (entry.type == SuggestionEntryType.FOREIGN_TAB) {
                            RecordUserAction.record("MobileCrossDeviceTabJourney");
                        }
                        mModuleDelegate.onUrlClicked(entry.url, getModuleType());
                    }
                };
        mMediator =
                new TabResumptionModuleMediator(
                        /* context= */ mContext,
                        /* moduleDelegate= */ mModuleDelegate,
                        /* tabModelSelectorSupplier= */ mTabModelSelectorSupplier,
                        /* model= */ mModel,
                        /* urlImageProvider= */ mUrlImageProvider,
                        /* reloadSessionCallback= */ this::updateModule,
                        /* statusChangedCallback= */ this::showModule,
                        /* seeMoreLinkClickCallback= */ this::onSeeMoreClicked,
                        suggstionClickCallback);
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

    PropertyModel getModelForTesting() {
        return mModel;
    }

    void onSeeMoreClicked() {
        mModuleDelegate.onUrlClicked(new GURL(UrlConstants.RECENT_TABS_URL), getModuleType());
    }
}
