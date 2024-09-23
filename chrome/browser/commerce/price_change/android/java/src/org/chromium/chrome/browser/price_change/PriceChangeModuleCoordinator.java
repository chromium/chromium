// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator for the price change module which can be embedded by surfaces like NTP or Start
 * surface.
 */
public class PriceChangeModuleCoordinator implements ModuleProvider {

    private final PriceChangeModuleMediator mMediator;

    /** Constructor. */
    public PriceChangeModuleCoordinator(
            Context context,
            Profile profile,
            TabModelSelector tabModelSelector,
            ModuleDelegate moduleDelegate) {
        PropertyModel model = new PropertyModel(PriceChangeModuleProperties.ALL_KEYS);
        mMediator =
                new PriceChangeModuleMediator(
                        context,
                        model,
                        profile,
                        tabModelSelector,
                        new FaviconHelper(),
                        ImageFetcherFactory.createImageFetcher(
                                ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                                profile.getProfileKey(),
                                GlobalDiscardableReferencePool.getReferencePool()),
                        moduleDelegate,
                        ContextUtils.getAppSharedPreferences());
    }

    /** Show price change module. */
    @Override
    public void showModule() {
        mMediator.showModule();
    }

    @Override
    public int getModuleType() {
        return mMediator.getModuleType();
    }

    @Override
    public void hideModule() {
        mMediator.destroy();
    }

    @Override
    public String getModuleContextMenuHideText(Context context) {
        return context.getString(R.string.price_change_module_context_menu_hide);
    }

    @Override
    public void onContextMenuCreated() {}
}
