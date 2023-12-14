// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import android.view.ViewStub;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the price change module which can be embedded by surfaces like NTP or Start
 * surface.
 */
public class PriceChangeModuleCoordinator {

    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final PriceChangeModuleMediator mMediator;

    /** Constructor. */
    public PriceChangeModuleCoordinator(
            ViewStub moduleViewStub, Profile profile, TabModelSelector tabModelSelector) {
        PropertyModel model = new PropertyModel(PriceChangeModuleProperties.ALL_KEYS);
        PriceChangeModuleView view = (PriceChangeModuleView) moduleViewStub.inflate();
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(model, view, new PriceChangeModuleViewBinder());
        mMediator =
                new PriceChangeModuleMediator(
                        view.getContext(),
                        model,
                        profile,
                        tabModelSelector,
                        new FaviconHelper(),
                        ImageFetcherFactory.createImageFetcher(
                                ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                                profile.getProfileKey(),
                                GlobalDiscardableReferencePool.getReferencePool()));
    }

    /** Show price change module. */
    public void showModule() {
        mMediator.showModule();
    }

    /** Destroy price change module. */
    public void destroy() {
        mModelChangeProcessor.destroy();
    }
}
